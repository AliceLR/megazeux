/*!
 * MegaZeux
 *
 * Copyright (C) 2018, 2019 Adrian Siekierka
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

const EPERM = 1;
const ENOENT = 2;
const EINVAL = 22;
const ENOTEMPTY = 39;
const O_CREAT = 0x40;
const O_TRUNC = 0x200;
const S_IFDIR = 0x4000;
const S_IFREG = 0x8000;
const S_IFMT = 0xF000;

function vfs_get_type(vfs, path) {
    if (path.length == 0) return "dir";
    let contents = vfs.get(path);
    if (contents !== null) {
        return "file";
    } else {
        const list = vfs.list(a => a.startsWith(path));
        if (list.length >= 1) return "dir";
    }
    return "empty";
}

function vfs_next_power_of_two(n) {
    var i = 4096;
    while (i < n) i *= 2;
    return i;
}

function vfs_expand_array(array, newLength) {
    if (newLength <= array.length) return array;
    newLength = vfs_next_power_of_two(newLength);

    var newArrayBuffer = new ArrayBuffer(newLength);
    var newArray = new Uint8Array(newArrayBuffer);
    newArray.set(array);
    return newArray;
}

export function wrapStorageForEmscripten(vfs) {
    let wrap;
    wrap = {
        mount: (mount) => {
            return wrap.createNode(null, '/', undefined, 0);
        },
        createNode: (parent, name, mode, dev) => {
            var vfs_path = name.substring(1);
            var vfs_type = vfs_get_type(vfs, vfs_path);
            // console.log("FS createNode " + name + " (" + vfs_type + ")");

            if (vfs_type == "empty") {
                throw new FS.ErrnoError(ENOENT);
            }

            if (vfs_type == "dir" && vfs_path.length >= 1 && !vfs_path.endsWith("/")) {
                vfs_path += "/";
            }

            if (mode === undefined) {
                if (vfs_type == "dir") mode = S_IFDIR | 0x1FF;
                else if (vfs_type == "file") mode = S_IFREG | 0x1FF;
            }

            var node_name = name.split("/");
            node_name = node_name[node_name.length - 1];

            var node = FS.createNode(parent, node_name, mode);
            node.vfs_path = vfs_path;
            node.vfs_type = vfs_type;
            node.vfs_time = new Date();
            node.node_ops = {};
            node.stream_ops = {};

            node.node_ops.getattr = (n) => {
                var attr = {
                    dev: 0,
                    ino: n.id,
                    mode: n.mode,
                    nlink: 1,
                    uid: 0,
                    gid: 0,
                    rdev: 0,
                    ctime: n.vfs_time,
                    mtime: n.vfs_time,
                    atime: n.vfs_time,
                    blksize: 4096
                }

                if (n.vfs_type == "dir") {
                    attr.size = 4096;
                } else {
                    attr.size = vfs.get(n.vfs_path).length;
                }

                attr.blocks = Math.ceil(attr.size / attr.blksize);
                return attr;
            }

            node.node_ops.setattr = (n, attr) => {
                if (attr.mode !== undefined) n.mode = attr.mode;
                if (attr.size !== undefined) {
                    // Used to implement O_TRUNC by the Emscripten FS API.
                    // console.log("FS setattr size " + n.vfs_path + " " + attr.size);
                    let old_data = vfs.get(n.vfs_path);
                    let new_data = new Uint8Array(attr.size);
                    if (attr.size > 0 && old_data) {
                        // Note: not sure sizes > 0 will reach here from the FS API...
                        new_data.set(old_data.slice(0, attr.size));
                    }
                    if (!vfs.set(n.vfs_path, new_data))
                        throw new FS.ErrnoError(EPERM);
                }
            }

            node.stream_ops.llseek = (stream, offset, whence) => {
                switch (whence) {
                    case 0:
                        return offset;
                    case 1:
                        return offset + stream.position;
                    case 2:
                        if (stream.vfs_data)
                            return offset + stream.vfs_data_length;
                        else
                            return offset;
                    default:
                        throw new FS.ErrnoError(EINVAL);
                }
            }

            if (node.vfs_type == "dir") {
                node.node_ops.lookup = (nparent, name) => {
                    return wrap.createNode(node, '/' + node.vfs_path + name, undefined, 0);
                };
                node.node_ops.mknod = (parent, name, mode, dev) => {
                    // console.log("FS mknod " + name + " " + mode);
                    if ((mode & S_IFMT) == S_IFREG) {
                        vfs.set(node.vfs_path + name, new Uint8Array(0));
                    } else {
                        throw new FS.ErrnoError(EPERM);
                    }
                    return wrap.createNode(parent, '/' + node.vfs_path + name, mode, dev);
                };
                node.node_ops.rename = (oldNode, newDir, newName) => {
                    console.log("FS FIXME rename " + newName);
                    throw new FS.ErrnoError(EPERM);
                };
                node.node_ops.unlink = (parent, name) => {
                    // console.log("FS unlink " + name);
                    if (!vfs.remove(node.vfs_path + name))
                        throw new FS.ErrnoError(EPERM);
                };
                node.node_ops.rmdir = (parent, name) => {
                    // console.log("FS rmdir " + name);
                    const path = node.vfs_path + name;
                    const list = vfs.list(a => a.startsWith(path));
                    for (var i = 0; i < list.length; i++) {
                        var entry = list[i].substring(path.length);
                        if (entry.length != 0)
                            throw new FS.ErrnoError(ENOTEMPTY);
                    }
                };
                node.node_ops.readdir = (node) => {
                    const path = node.vfs_path;
                    const list = vfs.list(a => a.startsWith(path));
                    let directories = {};
                    let dirents = [".", ".."];
                    for (var i = 0; i < list.length; i++) {
                        var entry = list[i].substring(path.length);
                        if (entry.length == 0) continue; // is empty
                        if (entry.indexOf('/') >= 0) {
                            // is directory
                            const entryDirName = entry.split('/')[0];
                            if (!directories.hasOwnProperty(entryDirName)) {
                                directories[entryDirName] = true;
                                dirents.push(entryDirName);
                            }
                        } else {
                            // is file
                            dirents.push(entry);
                        }
                    }
                    // console.log("FS readdir " + path + " => " + dirents.length + " dirents");
                    // console.log(dirents);
                    return dirents;
                };
            } else if (node.vfs_type == "file") {
                node.stream_ops.open = (stream) => {
                    // console.log("FS open " + node.vfs_path);
                    stream.vfs_modified = false;
                    if ((stream.flags & O_TRUNC) != 0) {
                        stream.vfs_data = new Uint8Array(0);
                    } else {
                        stream.vfs_data = vfs.get(node.vfs_path);
                        if (!stream.vfs_data) {
                            if ((stream.flags & O_CREAT) != 0) {
                                stream.vfs_data = new Uint8Array(0);
                            } else {
                                throw new FS.ErrnoError(ENOENT);
                            }
                        }
                    }
                    stream.vfs_data_length = stream.vfs_data.length;
                }

                node.stream_ops.close = (stream) => {
                    // console.log("FS close " + node.vfs_path);
                    if (stream.vfs_data && stream.vfs_modified) {
                        let length = Math.min(stream.vfs_data.length, stream.vfs_data_length);
                        vfs.set(node.vfs_path, stream.vfs_data.subarray(0, length));
                    }
                }

                node.stream_ops.read = (stream, buffer, bufOffset, dataLength, dataOffset) => {
                    // console.log("FS read " + node.vfs_path + " " + dataOffset + " " + dataLength);
                    const size = Math.min(dataLength, stream.vfs_data_length - dataOffset);
                    if (size > 8 && buffer.set && stream.vfs_data.subarray) {
                        buffer.set(stream.vfs_data.subarray(dataOffset, dataOffset + size), bufOffset);
                    } else {
                        for (var i = 0; i < size; i++) {
                            buffer[bufOffset + i] = stream.vfs_data[dataOffset + i];
                        }
                    }
                    return size;
                }

                node.stream_ops.write = (stream, buffer, bufOffset, dataLength, dataOffset) => {
                    // console.log("FS write " + node.vfs_path + " " + dataOffset + " " + dataLength);
                    if (dataLength <= 0) return 0;
                    stream.vfs_data = vfs_expand_array(stream.vfs_data, dataOffset + dataLength);
                    const size = dataLength;
                    if (size > 8 && stream.vfs_data.set && buffer.subarray) {
                        stream.vfs_data.set(buffer.subarray(bufOffset, bufOffset + size), dataOffset);
                    } else {
                        for (var i = 0; i < size; i++) {
                            stream.vfs_data[dataOffset + i] = buffer[bufOffset + i];
                        }
                    }
                    stream.vfs_data_length = Math.max(dataOffset + dataLength, stream.vfs_data_length);
                    stream.vfs_modified = true;
                    return dataLength;
                }

                node.stream_ops.allocate = (stream, offset, length) => {
                    stream.vfs_data = vfs_expand_array(stream.vfs_data, offset + length);
                    if (offset + length > stream.vfs_data_length) {
                        stream.vfs_data_length = offset + length;
                        stream.vfs_modified = true;
                    }
                }
            }
            return node;
        }
    };
    return wrap;
}
