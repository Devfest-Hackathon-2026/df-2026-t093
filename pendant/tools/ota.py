#!/usr/bin/env python3
"""BLE OTA for the pendant through MCUmgr/SMP (issue #11), dev tool. Needs bleak and smpclient.
Run it from Terminal.app on macOS (Bluetooth permission is per app).

  ota.py list                          show the image slots and their state
  ota.py swap [--reset]                mark the image in slot 1 for a test boot (a swap back to the previous version)
  ota.py upload fw/build/fw/zephyr/zephyr.signed.bin [--no-reset]
                                       upload to the secondary slot, mark it for a test boot, reset;
                                       the new image confirms itself after PENDANT_OTA_CONFIRM_S seconds
"""
import argparse, asyncio, sys, time

async def run(args):
    from bleak import BleakScanner
    from smpclient import SMPClient
    from smpclient.generics import error, success
    from smpclient.mcuboot import ImageInfo
    from smpclient.requests.image_management import ImageStatesRead, ImageStatesWrite
    from smpclient.requests.os_management import ResetWrite
    from smpclient.transport.ble import SMPBLETransport

    dev = await BleakScanner.find_device_by_name(args.name, timeout=args.scan)
    if not dev:
        sys.exit(f"no device named {args.name}")

    async def states(client):
        r = await client.request(ImageStatesRead())
        if error(r):
            sys.exit(f"image state read failed: {r}")
        for i in r.images:
            print(f"  slot {i.slot} image {i.image}: version {i.version} hash {i.hash.hex()[:12]} "
                  f"active={i.active} confirmed={i.confirmed} pending={i.pending} permanent={i.permanent}")
        return r.images

    from smpclient.exceptions import SMPUploadError
    client = SMPClient(SMPBLETransport(), dev.address)
    await client.connect(timeout_s=20)
    try:
        print("before:")
        await states(client)
        if args.cmd == "swap":
            other = [i for i in await states(client) if i.slot == 1]
            r = await client.request(ImageStatesWrite(hash=other[0].hash, confirm=False))
            print("marked slot 1 for a test boot:", "ok" if success(r) else r)
            if args.reset:
                await client.request(ResetWrite())
        if args.cmd == "upload":
            info = ImageInfo.load_file(args.file)
            data = open(args.file, "rb").read()
            print(f"image {info.header.ver}, {len(data)} B")
            t0, last = time.time(), 0
            first = None
            async for off in client.upload(data, slot=0, first_timeout_s=60):
                first = off if first is None else first
                if off == first:  # one chunk is ~2 KB: a first offset far beyond that means the device kept an earlier upload
                    print(f"  first accepted offset {off} B" + (" (resuming an earlier upload)" if off > 8192 else ""))
                if off - last >= 16384 or off == len(data):
                    print(f"  {off}/{len(data)} B  {(off - first) / (time.time() - t0) / 1000:.1f} kB/s")
                    last = off
                if args.stop_after and off >= args.stop_after:
                    print(f"test: dropping the link at {off} B")
                    return
            print(f"uploaded in {time.time() - t0:.1f} s")
            secondary = [i for i in await states(client) if i.slot == 1]
            if not secondary:
                sys.exit("the uploaded image does not show up in slot 1")
            # test boot: MCUboot reverts at the next reset unless the new image confirms itself
            r = await client.request(ImageStatesWrite(hash=secondary[0].hash, confirm=False))
            if error(r):
                sys.exit(f"could not mark the image for test: {r}")
            print("marked for a test boot; after:")
            await states(client)
            if not args.no_reset:
                await client.request(ResetWrite())
                print("reset requested")
    except SMPUploadError as e:
        rc = getattr(getattr(e.args[0] if e.args else None, "__class__", None), "__name__", "")
        sys.exit(f"the pendant refused the upload: {str(e)[:300]}")
    finally:
        try:
            await client.disconnect()
        except Exception:
            pass

def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("list", "upload", "swap"):
        s = sub.add_parser(name)
        s.add_argument("--name", default="LifeOS-Pendant")
        s.add_argument("--scan", type=float, default=15)
        if name == "swap":
            s.add_argument("--reset", action="store_true")
        if name == "upload":
            s.add_argument("file")
            s.add_argument("--no-reset", action="store_true")
            s.add_argument("--stop-after", type=int, default=0, help="test: stop (as if the link dropped) after this many bytes")
    asyncio.run(run(ap.parse_args()))

if __name__ == "__main__":
    main()
