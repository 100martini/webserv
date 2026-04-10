# webserv 🕷️

> http/1.1 from scratch. no libhttp. no cheat codes. just c++98 and trust.

a fully custom http server built in c++98, inspired by nginx's config syntax. handles real requests, real uploads, real cgi scripts — and it won't leak memory on you.

---

## features

- `GET` `POST` `DELETE` — the holy trinity
- nginx-style config file (multiple servers, multiple locations)
- cgi/1.1 support — python, php, whatever you throw at it
- chunked transfer encoding
- multipart file uploads
- directory autoindex
- custom error pages
- redirects (3xx)
- request timeouts + cgi timeout (5s hard kill)
- non-blocking i/o with `epoll`
- keep-alive connections

---

## architecture

```
main.cpp
├── parsing/        ← config file parser (nginx-like syntax)
│   ├── Config      ← data structures (ServerConfig, LocationConfig)
│   ├── Parser      ← tokenizer + validator
│   ├── Location    ← location block parser
│   └── Utils       ← trim, split, parseSize, host validation
│
├── server/         ← the engine
│   ├── Server      ← event loop, epoll, cgi orchestration
│   ├── Socket      ← tcp socket wrapper
│   ├── Client      ← per-connection state machine
│   ├── EventManager← epoll abstraction
│   └── CGIHelpers  ← fork/exec, env setup, pipe i/o
│
└── http/           ← the protocol
    ├── HttpParser  ← request line + headers + body parser
    ├── HttpRequest ← validated request object
    ├── HttpResponse← response builder (status, headers, body)
    ├── Methods     ← GET/POST/DELETE handlers
    └── HelpersMethods ← file serving, directory listing, uploads
```

---

## build & run

```bash
make              # build
./webserv         # run with default webserv.conf
./webserv my.conf # run with custom config
make test         # build + run with webserv.conf
make clean        # remove objects
make fclean       # remove objects + binary
make re           # fclean + make
```

---

## config syntax

```nginx
server {
    listen 127.0.0.1:8080;
    server_name mysite.com;
    client_max_body_size 10M;
    error_page 404 www/errors/404.html;

    location / {
        methods GET POST;
        root www/html;
        index index.html;
    }

    location /upload {
        methods GET POST DELETE;
        root www/upload;
        upload_path www/upload;
        autoindex on;
        client_max_body_size 20M;
    }

    location /cgi-bin {
        methods GET POST;
        root www/cgi-bin;
        cgi .py /usr/bin/python3;
        cgi .php /usr/bin/php-cgi;
    }

    location /old {
        return 301 /new;
    }
}
```

| directive | what it does |
|---|---|
| `listen` | `host:port` or just `port` |
| `server_name` | virtual host name |
| `client_max_body_size` | supports K, M, G suffixes |
| `error_page` | custom error pages per code |
| `methods` | GET POST DELETE (default: GET) |
| `root` | filesystem root for this location |
| `index` | default file(s) to serve |
| `autoindex` | on/off directory listing |
| `upload_path` | where uploaded files land |
| `cgi` | `.ext /path/to/interpreter` |
| `return` | redirect with 3xx code |

---

## project structure

```
.
├── Makefile
├── webserv.conf
├── main.cpp
├── parsing/
├── server/
├── http/
└── www/
    ├── html/         ← served at /
    ├── upload/       ← served at /upload
    ├── static/       ← served at /static
    ├── cgi-bin/      ← served at /cgi-bin
    │   └── tests/    ← cgi test scripts
    └── errors/       ← custom error pages
```

---

made at **1337 benguerir** · 42 network  
`wel-kass` · [intra](https://profile.intra.42.fr/users/wel-kass)
