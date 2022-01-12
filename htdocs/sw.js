// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

const CACHE = 'myMPD-cache-v9.1.0';
const subdir = self.location.pathname.replace('/sw.js', '').replace(/\/$/, '');
const urlsToCache = [
    subdir + '/',
    subdir + '/css/combined.css',
    subdir + '/js/combined.js',
    subdir + '/assets/appicon-192.png',
    subdir + '/assets/appicon-512.png',
    subdir + '/assets/coverimage-stream.svg',
    subdir + '/assets/coverimage-notavailable.svg',
    subdir + '/assets/coverimage-loading.svg',
	subdir + '/assets/coverimage-mympd.svg',
    subdir + '/assets/favicon.ico',
    subdir + '/assets/MaterialIcons-Regular.woff2',
	subdir + '/assets/mympd-background-dark.svg',
	subdir + '/assets/mympd-background-light.svg'
];

const ignoreRequests = new RegExp(subdir + '/(' + [
	'api/(.*)',
	'ca.crt',
	'ws/',
	'stream/',
	'albumart/(.*)',
	'tagart/(.*)',
	'browse/(.*)'].join('|') + ')$');

self.addEventListener('install', function(event) {
    event.waitUntil(
        caches.open(CACHE).then(function(cache) {
            urlsToCache.map(function(url) {
				return cache.add(url).catch(function (reason) {
                    return console.log('ServiceWorker: ' + String(reason) + ' ' + url);
                });
            });
        })
    );
});

self.addEventListener('fetch', function(event) {
    if (event.request.url.indexOf('https://' + self.location.hostname + '/') !== 0 ||
        ignoreRequests.test(event.request.url))
    {
        return false;
    }
    event.respondWith(
        caches.match(event.request).then(function(response) {
            if (response) {
                return response;
            }
            else {
                return fetch(event.request);
            }
        })
    );
});

self.addEventListener('activate', function(event) {
    event.waitUntil(
        caches.keys().then(function(cacheNames) {
            return Promise.all(
                cacheNames.map(function(cacheName) {
                    if (cacheName !== CACHE) {
                        return caches.delete(cacheName);
                    }
                })
            );
        })
    );
});
