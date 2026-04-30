# Systemd Examples

These examples show how to run Zereh deployment and health checks under systemd.

## Packaged Example Files

Use the repository-provided templates:

- docs/operators/systemd/zereh-load.service
- docs/operators/systemd/zereh-healthcheck.service
- docs/operators/systemd/zereh-healthcheck.timer
- docs/operators/systemd/zereh.env.example

## Install Steps

1. Copy deployment tree to a stable location (for example `/opt/zereh`).
2. Copy service files into `/etc/systemd/system/`.
3. Create `/etc/zereh/zereh.env` from `zereh.env.example`.
4. Reload systemd and enable units.

```bash
sudo mkdir -p /etc/zereh
sudo cp /opt/zereh/docs/operators/systemd/zereh-*.service /etc/systemd/system/
sudo cp /opt/zereh/docs/operators/systemd/zereh-healthcheck.timer /etc/systemd/system/
sudo cp /opt/zereh/docs/operators/systemd/zereh.env.example /etc/zereh/zereh.env
sudo systemctl daemon-reload
sudo systemctl enable --now zereh-load.service
sudo systemctl enable --now zereh-healthcheck.timer
```

## Verify

```bash
sudo systemctl status zereh-load.service
sudo systemctl status zereh-healthcheck.timer
sudo journalctl -u zereh-load.service -n 100 --no-pager
```

## Operational Notes

- Update paths in service files before production use.
- `ExecStop` in `zereh-load.service` uses `zerehctl unload` for clean detach.
- If shell wrappers are restricted on your host, replace `bash -lc` with explicit absolute command paths.
- To emit structured logs from loader and scripts, set `ZEREH_LOG_MODE=structured` in `/etc/zereh/zereh.env`.
