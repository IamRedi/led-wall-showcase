"""Entry point: ``python3 -m ha_adapter --config /path/to/config.json``."""

from __future__ import annotations

import argparse
import logging
import signal
import sys

from .adapter import AdapterStopped, LedWallHaAdapter
from .config import ConfigError, load_config


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        prog="ha_adapter",
        description="Home Assistant external adapter for LED Wall (LED-L005).",
    )
    parser.add_argument(
        "--config",
        default=None,
        help="path to the adapter JSON config (default: $LEDWALL_HA_CONFIG or ha_adapter/config.json)",
    )
    args = parser.parse_args(argv)

    try:
        config = load_config(args.config)
    except ConfigError as exc:
        print(f"config error: {exc}", file=sys.stderr)
        return 2

    logging.basicConfig(
        level=getattr(logging, config.log_level),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        stream=sys.stderr,
    )

    adapter = LedWallHaAdapter(config)

    def _stop_handler(signum, _frame):
        logging.getLogger("ledwall_ha").info("received signal %s, stopping", signum)
        adapter.stop()

    signal.signal(signal.SIGTERM, _stop_handler)

    try:
        adapter.run()
    except KeyboardInterrupt:
        adapter.stop()
        return 0
    except AdapterStopped as exc:
        logger = logging.getLogger("ledwall_ha")
        logger.info("%s", exc)
        return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
