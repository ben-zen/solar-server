# Solar Server Logger

Drawing direct inspiration from the way [LOW←TECH MAGAZINE](https://solar.lowtechmagazine.com/power/) works, and how they have a power status page, I decided that since this is an art installation with very similar goals, it should have an update mechanism. However, where their solution is a totally realtime mechanism, I've opted for the slightly less realtime but also... less computational approach of updating a quasi-static CSS file. This binary is the generator 

## Building the logger

### Dependencies

#### Fedora

`libcurl`, `libcurl-devel`, `fmt`, `fmt-devel`

#### Debian

`libcurl4`, `libcurl4-dev`, `libfmt`, `libfmt-dev`

## Deploying the logger

I may well write a setup script to handle deployment. Until then, the layout should be the following:

```
/srv/logger:
logger update-status.sh

/etc/systemd/system:
update-status.service update-status.timer
```

```sh
systemctl daemon-reload
systemctl enable --now update-status.timer
```

## Other works

### nlohmann's json.hpp

I'm using [nlohmann's json library](https://github.com/nlohmann/json) to handle json parsing.
