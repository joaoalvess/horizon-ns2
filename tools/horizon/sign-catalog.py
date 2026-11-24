#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import base64
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import time
import zipfile

TITLE = 'PPSA98515'
NOTES_LIMIT = 4000
MAX_APP_FILE = 65536
MAX_MANIFEST = 512 * 1024
MAX_ARCHIVE = 0x80000000
ED25519_PUBLIC_DER = bytes.fromhex('302a300506032b6570032100')
VERSION = re.compile(r'\d{2}\.\d{3}\.\d{3}')


def parts(version):
    return tuple(int(n) for n in version.split('.'))


def compiled_keys(source):
    block = re.search(r'self_update_keys\[2\]\[32\]\s*=\s*\{(.*?)\};', source.read_text(), re.S)
    raw = bytes(int(b, 16) for b in re.findall(r'0x([0-9a-fA-F]{2})', block.group(1))) if block else b''
    if len(raw) != 64:
        sys.exit(f'{source}: self_update_keys is not two 32-byte keys')
    return raw[:32], raw[32:]


def verifies(openssl, key, manifest, signature):
    pem = base64.encodebytes(ED25519_PUBLIC_DER + key).decode()
    with tempfile.NamedTemporaryFile('w', suffix='.pem') as public:
        public.write(f'-----BEGIN PUBLIC KEY-----\n{pem}-----END PUBLIC KEY-----\n')
        public.flush()
        return subprocess.run([openssl, 'pkeyutl', '-verify', '-pubin', '-inkey', public.name, '-rawin',
                               '-in', str(manifest), '-sigfile', str(signature)],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0


def main():
    parser = argparse.ArgumentParser(description='Writes and signs the Horizon update catalog for one release ZIP.')
    parser.add_argument('--zip', required=True, type=pathlib.Path, help='EglantinePS5-vX.YYY.ZZZ.zip of the release')
    parser.add_argument('--repo', required=True, help='owner/name of the repository the release is on')
    parser.add_argument('--key', required=True, type=pathlib.Path, help='Ed25519 private key (PEM)')
    parser.add_argument('--keys-from', required=True, type=pathlib.Path, help='self_update.c, whose keys must verify the signature')
    parser.add_argument('--out', required=True, type=pathlib.Path, help='folder that gets manifest.json, manifest.sig and apps/')
    parser.add_argument('--notes', type=pathlib.Path, help='release notes as plain text')
    parser.add_argument('--released', default='', help='publication time of the release (ISO 8601)')
    parser.add_argument('--prerelease', action='store_true')
    parser.add_argument('--current', type=pathlib.Path, help='the app file the catalog offers now, if any')
    parser.add_argument('--sequence', type=int, default=int(time.time()))
    parser.add_argument('--openssl', default='openssl')
    args = parser.parse_args()

    with zipfile.ZipFile(args.zip) as archive:
        param = json.loads(archive.read(f'{TITLE}/sce_sys/param.json'))
    version = param.get('contentVersion', '')
    if param.get('titleId') != TITLE or not VERSION.fullmatch(version):
        sys.exit(f'{args.zip.name}: param.json has titleId {param.get("titleId")!r}, contentVersion {version!r}')
    tag = 'v' + version.removeprefix('0')
    if args.zip.name != f'EglantinePS5-{tag}.zip':
        sys.exit(f'{args.zip.name}: the package inside is {version}, so the ZIP must be EglantinePS5-{tag}.zip')
    if args.current and args.current.exists():
        offered = json.loads(args.current.read_text()).get('content_version', '')
        if VERSION.fullmatch(offered) and parts(offered) > parts(version):
            sys.exit(f'the catalog already offers {offered}, newer than {version}')

    data = args.zip.read_bytes()
    if len(data) > MAX_ARCHIVE:
        sys.exit(f'{args.zip.name}: {len(data)} bytes, over the 2 GiB the updater accepts')
    notes = args.notes.read_text().replace('\r\n', '\n').strip() if args.notes else ''
    truncated = len(notes) > NOTES_LIMIT
    if truncated:
        notes = notes[:NOTES_LIMIT].rsplit('\n', 1)[0].rstrip()
    names = param.get('localizedParameters', {})
    name = names.get(names.get('defaultLanguage', 'en-US'), {}).get('titleName', TITLE)
    release_url = f'https://github.com/{args.repo}/releases/tag/{tag}'
    app = {
        'schema': 3,
        'id': TITLE,
        'titleid': TITLE,
        'name': name,
        'kind': 'app',
        'version': tag[1:],
        'source_repo': f'https://github.com/{args.repo}',
        'artifact_url': f'https://github.com/{args.repo}/releases/download/{tag}/{args.zip.name}',
        'sha256': hashlib.sha256(data).hexdigest(),
        'status': 'available',
        'content_version': version,
        'format': 'zip',
        'artifact_name': args.zip.name,
        'size': len(data),
        'tag': tag,
        'released': args.released,
        'prerelease': args.prerelease,
        'release_url': release_url,
        'page': release_url,
        'release_notes': notes,
        'release_notes_truncated': truncated,
    }
    app_bytes = (json.dumps(app, ensure_ascii=False, separators=(',', ':')) + '\n').encode()
    manifest = {'schema': 3, 'sequence': args.sequence,
                'files': {f'apps/{TITLE}.json': hashlib.sha256(app_bytes).hexdigest()}}
    manifest_bytes = (json.dumps(manifest, separators=(',', ':')) + '\n').encode()
    if len(app_bytes) > MAX_APP_FILE or len(manifest_bytes) > MAX_MANIFEST:
        sys.exit(f'app file {len(app_bytes)} bytes, manifest {len(manifest_bytes)} bytes: over the updater limits')

    (args.out / 'apps').mkdir(parents=True, exist_ok=True)
    (args.out / 'apps' / f'{TITLE}.json').write_bytes(app_bytes)
    manifest_path = args.out / 'manifest.json'
    signature_path = args.out / 'manifest.sig'
    manifest_path.write_bytes(manifest_bytes)
    subprocess.run([args.openssl, 'pkeyutl', '-sign', '-rawin', '-inkey', str(args.key),
                    '-in', str(manifest_path), '-out', str(signature_path)], check=True)
    if signature_path.stat().st_size != 64:
        sys.exit(f'manifest.sig is {signature_path.stat().st_size} bytes, not a raw Ed25519 signature')
    if not any(verifies(args.openssl, key, manifest_path, signature_path) for key in compiled_keys(args.keys_from)):
        sys.exit(f'manifest.sig does not verify with either key in {args.keys_from}')
    print(f'{tag}: content_version {version}, {len(data)} bytes, sha256 {app["sha256"]}, sequence {args.sequence}')


if __name__ == '__main__':
    main()
