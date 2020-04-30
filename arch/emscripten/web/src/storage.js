/*!
 * MegaZeux
 *
 * Copyright (C) 2018, 2019 Adrian Siekierka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import { getIndexedDB, getLocalStorage, drawErrorMessage, xhrFetchAsArrayBuffer } from "./util";
import { zip } from "./zip.js";

function filterKeys(list, filter) {
	if (filter == null) return list;

	let newList = [];
	for (var i = 0; i < list.length; i++) {
		const key = list[i];
		if (filter(key)) newList.push(key);
	}
	return newList;
}

class InMemoryStorage {
	constructor(inputMap, options) {
		this.map = {};
		for (var key in inputMap) {
			this.map[key] = inputMap[key];
		}
		this.readonly = (options && options.readonly) || false;
	}

	canSet(key) {
		return this.readonly;
	}

	get(key) {
		if (!this.map.hasOwnProperty(key)) {
			return null;
		}
		return this.map[key].slice(0);
	}

	list(filter) {
		return filterKeys(Object.keys(this.map), filter);
	}

	set(key, value) {
		if (this.readonly) return false;
		this.map[key] = value;
		return true;
	}

	remove(key) {
		if (this.readonly) return false;
		delete this.map[key];
		return true;
	}
}

class CompositeStorage {
	constructor(providers) {
		this.providers = providers;
	}

	canSet(key) {
		for (var p = 0; p < this.providers.length; p++) {
			let provider = this.providers[p];
			if (provider.canSet(key)) {
				return true;
			}
		}
		return false;
	}

	get(key) {
		for (var p = this.providers.length - 1; p >= 0; p--) {
			let provider = this.providers[p];
			const result = provider.get(key);
			if (result != null) return result;
		}
		return null;
	}

	list(filter) {
		let data = [];
		for (var p = 0; p < this.providers.length; p++) {
			let provider = this.providers[p];
			data = data.concat(provider.list(filter));
		}
		return data.sort().filter(function(item, i, sortedData) {
			return (i <= 0) || (item != sortedData[i - 1]);
		});
	}

	set(key, value) {
		for (var p = this.providers.length - 1; p >= 0; p--) {
			let provider = this.providers[p];
			if (provider.set(key, value)) return true;
		}
		return false;
	}

	remove(key) {
		for (var p = this.providers.length - 1; p >= 0; p--) {
			let provider = this.providers[p];
			if (provider.remove(key)) return true;
		}
		return false;
	}
}

class AsyncStorageWrapper extends InMemoryStorage {
	constructor(parent) {
		super({}, {});
		this.parent = parent;
	}

	_populate() {
		const self = this;

		return this.parent.list(null).then(result => {
			self.map = {};

			let getters = [];
			for (var i = 0; i < result.length; i++) {
				const key = result[i];
				getters.push(self.parent.get(key).then(result => {
					self.map[key] = result;
				}));
			}

			return Promise.all(getters);
		});
	}

	set(key, value) {
		if (super.set(key, value)) {
			this.parent.set(key, value);
			return true;
		} else {
			return false;
		}
	}

	remove(key) {
		if (super.remove(key)) {
			this.parent.remove(key);
			return true;
		} else {
			return false;
		}
	}
}

class BrowserBackedStorage {
	constructor(localStorage, prefix) {
		this.storage = localStorage;
		this.prefix = prefix + "_file_";
	}

	canSet(key) {
		return true;
	}

	get(key) {
		const result = this.storage.getItem(this.prefix + key);
		if (result !== null) {
			return result.split(",").map(s => parseInt(s));
		} else {
			return null;
		}
	}

	list(filter) {
		var list = [];
		for (var i = 0; i < this.storage.length; i++) {
			const key = this.storage.key(i);
			if (key.startsWith(this.prefix)) {
				list.push(key.substring(this.prefix.length));
			}
		}
		return filterKeys(list, filter);
	}

	set(key, value) {
		this.storage.setItem(this.prefix + key, value.join(","));
		return true;
	}

	remove(key) {
		this.storage.removeItem(this.prefix + key);
		return true;
	}
}

class IndexedDbBackedAsyncStorage {
	constructor(indexedDB, dbName, options) {
		this.indexedDB = indexedDB;
		this.dbName = dbName;
	}

	_open() {
		const self = this;
		return new Promise((resolve, reject) => {
			var dbRequest = self.indexedDB.open(this.dbName, 1);
			dbRequest.onupgradeneeded = event => {
				self.database = event.target.result;
				self.files = self.database.createObjectStore("files", {"keyPath": "filename"});
			}
			dbRequest.onsuccess = event => {
				self.database = dbRequest.result;
				resolve();
			}
			dbRequest.onerror = event => {
				reject("Failed to open IndexedDB database (is this a private window?)");
			}
		});
	}

	canSet(key) {
		return Promise.resolve(true);
	}

	get(key) {
		const transaction = this.database.transaction(["files"], "readonly");
		return new Promise((resolve, reject) => {
			const request = transaction.objectStore("files").get(key);
			request.onsuccess = event => {
				if (request.result && request.result.value) resolve(request.result.value);
				else resolve(null);
			}
			request.onerror = event => {
				resolve(null);
			}
		});
	}

	list(filter) {
		const transaction = this.database.transaction(["files"], "readonly");
		return new Promise((resolve, reject) => {
			const store = transaction.objectStore("files");
			if (typeof store.getAllKeys === 'function') {
				const request = store.getAllKeys();
				request.onsuccess = event => {
					resolve(filterKeys(request.result, filter));
				}
				request.onerror = event => {
					resolve([]);
				}
			} else {
				var result = [];
				var request;
				if (typeof store.openKeyCursor === 'function') {
					request = store.openKeyCursor();
				} else {
					request = store.openCursor();
				}
				request.onsuccess = event => {
					var cursor = event.target.result;
					if (cursor) {
						result.push(cursor.key);
						cursor.continue();
					} else {
						resolve(filterKeys(result, filter));
					}
				}
				request.onerror = event => {
					resolve(result);
				}
			}
		});
	}

	set(key, value) {
		const transaction = this.database.transaction(["files"], "readwrite");
		return new Promise((resolve, reject) => {
			const request = transaction.objectStore("files").put({
				"filename": key,
				"value": value
			});
			request.onsuccess = event => {
				resolve(true);
			}
			request.onerror = event => {
				resolve(false);
			}
		});
	}

	remove(key) {
		const transaction = this.database.transaction(["files"], "readwrite");
		return new Promise((resolve, reject) => {
			const request = transaction.objectStore("files").delete(key);
			request.onsuccess = event => {
				resolve(true);
			}
			request.onerror = event => {
				resolve(false);
			}
		});
	}
}

export function createBrowserBackedStorage(storage, dbName) {
	return new BrowserBackedStorage(storage, dbName);
}

export function createIndexedDbBackedAsyncStorage(dbName) {
	const indexedDB = getIndexedDB();
	if (!indexedDB) {
		return Promise.reject("IndexedDB not supported!");
	}
	const dbObj = new IndexedDbBackedAsyncStorage(indexedDB, dbName);
	return dbObj._open().then(_ => dbObj);
}

export function wrapAsyncStorage(asyncVfs) {
	const obj = new AsyncStorageWrapper(asyncVfs);
	return obj._populate().then(_ => obj);
}

export function createInMemoryStorage(inputMap, options) {
	return new InMemoryStorage(inputMap, options);
}

export function createCompositeStorage(providers) {
	return new CompositeStorage(providers);
}

export function createZipStorage(url, options, progressCallback) {
	return xhrFetchAsArrayBuffer(url, progressCallback)
		.then(xhr => new Promise((resolve, reject) => {
			let files;
			let fileMap = {};

			try
			{
				files = zip.extract(xhr.response);
			}
			catch(e)
			{
				reject(e);
				return;
			}

			for (var key in files) {
				const keyOrig = key;

				if (options && options.filenameMap) {
					if (typeof(options.filenameMap) === "object") {
						key = options.filenameMap[key] || undefined;
					} else if (typeof(options.filenameMap) === "string") {
						key = options.filenameMap + key;
					} else if (typeof(options.filenameMap) === "function") {
						key = options.filenameMap(key);
					}
				}

				if (key) {
					if (!(options && options.filenameFilter) || options.filenameFilter(key)) {
						fileMap[key] = files[keyOrig];
					}
				}
			}

			resolve(new InMemoryStorage(fileMap, options));
		}));
}
