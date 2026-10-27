"""Entry point: python3 -m hugo_adapter [--config FILE] [--device URL].

Runs the HUGO DeviceAdapter stdio JSON-RPC service (line-delimited UTF-8
JSON: methods ping, tools/list, tools/call). Responses go to stdout; all
logging goes to stderr so stdout stays pure protocol. Exit codes: 0 on EOF/
SIGTERM, 2 on configuration error.
"""

from __future__ import annotations

import argparse
import logging
import signal
import sys

from .config import ConfigError, load_config
from .device import DeviceClient
from .service import DeviceAdapterService


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="hugo_adapter",
        description="LED Wall HUGO DeviceAdapter — intent-only JSON-RPC tool surface over the LED Wall semantic API (frozen spec §16).",
    )
    parser.add_argument("--config", default=None, help="JSON config file (default: $LEDWALL_HUGO_CONFIG or hugo_adapter/config.json)")
    parser.add_argument("--device", default=None, help="Override the configured device base URL (http://...)")
    return parser


def main(argv=None) -> int:
    args = _build_parser().parse_args(argv)
    try:
        config = load_config(args.config)
    except ConfigError as exc:
        print(f"hugo_adapter: configuration error: {exc}", file=sys.stderr)
        return 2
    if args.device is not None:
        device_url = args.device.rstrip("/")
        if not device_url.startswith("http://"):
            print("hugo_adapter: --device must be an http:// URL in v1", file=sys.stderr)
            return 2
        config = type(config)(
            device_url=device_url,
            device_timeout_s=config.device_timeout_s,
            capability_ttl_s=config.capability_ttl_s,
            log_level=config.log_level,
            warnings=config.warnings,
        )

    logging.basicConfig(
        stream=sys.stderr,
        level=getattr(logging, config.log_level, logging.INFO),
        format="%(asctime)s %(levelname)s hugo_adapter %(message)s",
    )
    log = logging.getLogger("hugo_adapter")
    for warning in config.warnings:
        log.warning("%s", warning)
    log.info("starting: device=%s timeout=%ss capability_ttl=%ss", config.device_url, config.device_timeout_s, config.capability_ttl_s)

    def _stop(signum, frame):  # noqa: ARG001 (signal hook)
        log.info("signal %s — stopping", signum)
        sys.exit(0)

    signal.signal(signal.SIGTERM, _stop)

    client = DeviceClient(config.device_url, config.device_timeout_s)
    service = DeviceAdapterService(client, config, log=log.warning)
    service.run()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ValueError as exc:  # bad --device URL (config errors exit 2 above)
        print(f"hugo_adapter: {exc}", file=sys.stderr)
        sys.exit(2)
