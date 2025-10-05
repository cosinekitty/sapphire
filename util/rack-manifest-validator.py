import sys
import os
import argparse
import json
import glob
import subprocess
import requests
import re
import urllib.request
import urllib.error

URL_KEYS = ["pluginUrl", "authorUrl", "manualUrl", "sourceUrl", "changelogUrl"]
SPDX_URL = "https://raw.githubusercontent.com/spdx/license-list-data/master/json/licenses.json"
RACK_V1_TAG_CPP_URL = "https://raw.githubusercontent.com/VCVRack/Rack/v1/src/tag.cpp"
RACK_V2_TAG_CPP_URL = "https://raw.githubusercontent.com/VCVRack/Rack/v2/src/tag.cpp"

REQUIRED_TOP_LEVEL_KEYS = [
    "slug",
    "name",
    "version",
    "license",
    "author"
]


REQUIRED_MODULE_KEYS = [
    "slug",
    "name"
]


INVALID_LICENSE_IDS = [
    "GPL-3.0"
]


class Version:
    """
    Version class that implements version comparison analogous to Rack.
    """
    def __init__(self, s):
        self.original = s
        self.parts = s.split('.')

    def __str__(self):
        return self.original

    def _key(self):
        def part_key(p):
            try:
                # Integers get (1, int_value) → higher priority
                return (1, int(p))
            except ValueError:
                # Strings get (0, str_value) → lower priority
                return (0, p)
        return [part_key(p) for p in self.parts]

    def __lt__(self, other):
        return self._key() < other._key()

    def __eq__(self, other):
        return self._key() == other._key()


def parse_args(argv):
    parser = argparse.ArgumentParser()

    parser.add_argument("plugin_root" , help="Root directory of plugins, e.g. library repo root", type=str)
    parser.add_argument("-p" , "--plugin", help="Specific plugin to validate", type=str)

    return parser.parse_args()


def get_submodule_sha(plugin_root, plugin_path):
    try:
        output = subprocess.check_output(["git", "ls-files", "-s", plugin_path],
            cwd=plugin_root,
            stderr=subprocess.STDOUT)
        return output.strip().decode("UTF-8") .split(" ")[1]
    except Exception:
        return None


def get_plugin_head_sha(plugin_path):
    try:
        output = subprocess.check_output(["git", "rev-parse", "HEAD"],
            cwd=plugin_path,
            stderr=subprocess.STDOUT)
        return output.strip().decode("UTF-8")
    except Exception:
        return None


def get_plugin_version(plugin_path, sha):
    try:
        output = subprocess.check_output(["git", "show", "%s:plugin.json" % sha],
            cwd=plugin_path,
            stderr=subprocess.STDOUT)
        pj = json.loads(output.strip().decode("UTF-8"))
        return pj["version"]
    except Exception:
        raise


def get_valid_tags(rack_tag_cpp_url):
    tag_cpp = requests.get(rack_tag_cpp_url).text
    tags = []
    capture_tags = False
    for line in tag_cpp.split("\n"):
        if "tagAliases" in line.strip():
            capture_tags = True
            continue
        if capture_tags and line.strip() == "};":
            capture_tags = False
            break
        if capture_tags:
            for t in line.strip().split("}")[0].split(","):
                tags.append(t.strip().replace('{','').replace('"','').lower())
    return tags


def get_spdx_license_ids():
    license_json = json.loads(
        requests.get(SPDX_URL).text
        )
    return [license["licenseId"] for license in license_json["licenses"]]


def validate_license_id(valid_license_ids, license_id):
    return (license_id in valid_license_ids and license_id not in INVALID_LICENSE_IDS)


def get_manifest_diff(repo_path, submodule_sha, head_sha):
    cmd = "git diff --word-diff %s %s plugin.json" % (submodule_sha, head_sha)
    return subprocess.check_output(cmd.split(" "), cwd=repo_path).decode("UTF-8")


def get_manifest_at_revision(repo_path, sha):
    cmd = "git show %s:plugin.json" % (sha)
    return subprocess.check_output(cmd.split(" "), cwd=repo_path).decode("UTF-8")


def check_for_plugin_slug_change(repo_path, submodule_sha, head_sha):
    prev_slug = json.loads(get_manifest_at_revision(repo_path, submodule_sha))["slug"]
    new_slug = json.loads(get_manifest_at_revision(repo_path, head_sha))["slug"]

    return (prev_slug != new_slug, new_slug)


def check_for_module_slug_changes(repo_path, submodule_sha, head_sha):
    prev_manifest = json.loads(get_manifest_at_revision(repo_path, submodule_sha))
    new_manifest = json.loads(get_manifest_at_revision(repo_path, head_sha))

    changed = False
    changed_slugs = []

    prev_slugs = [module["slug"] for module in prev_manifest["modules"]]
    new_slugs = [module["slug"] for module in new_manifest["modules"]]

    for index, slug in enumerate(prev_slugs):
        if slug not in new_slugs:
            # Slug removal detected. That's only OK if slug was disabled in previous manifest.
            if not "disabled" in prev_manifest["modules"][index].keys() or \
               prev_manifest["modules"][index]["disabled"] != True:
                changed_slugs.append(slug)
                changed = True

    return (changed, changed_slugs)


def validate_tags(tags, valid_tags):
    invalid_tags = []
    for tag in tags:
        if tag.lower() not in valid_tags:
            invalid_tags.append(tag)
    return invalid_tags if invalid_tags else None


def is_valid_url(url):
    # File-type URLs are not allowed in the plugin manifest.
    if url.startswith("file:"):
        return 1

    headers = {
        'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36',
        'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
        'Accept-Language': 'en-US,en;q=0.5',
        'Accept-Encoding': 'gzip, deflate',
        'Referer': 'https://www.google.com',
        'Connection': 'keep-alive'
    }
    request = urllib.request.Request(url, headers=headers)

    try:
        with urllib.request.urlopen(request, timeout=5) as response:
            status_code = response.status
            return 200 <= status_code < 400
    except urllib.error.HTTPError as e:
        print(f"HTTPError: {e.code} - {e.reason}")
    except urllib.error.URLError as e:
        print(f"URLError: {e.reason}")
    except Exception as e:
        print(f"Unexpected error: {e}")
    return False


def is_valid_slug(slug):
    for c in slug:
        if not (c.isalnum() or c == '-' or c == '_'):
            return False
    return True


def main(argv=None):

    args = parse_args(argv)
    plugin_root = args.plugin_root

    try:

        failed = False

        if not os.path.exists(plugin_root):
            raise Exception("Invalid Plugin root: %s" % plugin_root)

        # Adjust plugin_root if we are in the library repository.
        repos_path = os.path.join(plugin_root, "repos")
        if os.path.exists(repos_path):
            plugin_root = repos_path

        if args.plugin:
            plugins = [os.path.join(plugin_root, args.plugin)]
        else:
            plugins = glob.glob(plugin_root+"/*")

        for plugin_path in sorted(plugins):
            plugin_name = os.path.basename(os.path.abspath(plugin_path)) # from path
            print("[%s] Validating plugin.json..." % plugin_name, end='', flush=True)

            plugin_json = None
            failed = False
            output = []

            try:
                if not os.path.exists(plugin_path):
                    raise Exception("Invalid plugin path: %s" % plugin_path)

                manifest = os.path.join(plugin_path, "plugin.json")
                with open(manifest, 'r', encoding='utf-8') as p:
                    plugin_json = json.load(p)

                plugin_version = plugin_json["version"]

                # Check that license string does not contain spaces
                if ' ' in plugin_version:
                    output.append("%s: invalid plugin version containing whitespace" % (plugin_version))
                    failed = True

                if int(plugin_version[0]) < 2:
                    rack_tag_cpp_url = RACK_V1_TAG_CPP_URL
                else:
                    rack_tag_cpp_url = RACK_V2_TAG_CPP_URL
                valid_tags = get_valid_tags(rack_tag_cpp_url)

                valid_license_ids = get_spdx_license_ids()

                # Validate top-level manifest keys
                for key in REQUIRED_TOP_LEVEL_KEYS:
                    if key not in plugin_json.keys():
                        output.append("Missing key: %s" % key)
                        failed = True

                # Validate plugin slug
                if not is_valid_slug(plugin_json["slug"]):
                    output.append("%s: invalid plugin slug" % (plugin_json["slug"]))
                    failed = True

                # Validate module entries
                modules = plugin_json["modules"]

                invalid_tag = False
                invalid_slug = False
                for module in modules:
                    for key in REQUIRED_MODULE_KEYS:
                        if key not in module.keys():
                            output.append("%s: missing key: %s" % (module["slug"], key))
                            failed = True

                    # Validate tags
                    if "tags" in module.keys():
                        res = validate_tags(module["tags"], valid_tags)
                        if res:
                            output.append("%s: invalid module tags: %s" % (module["slug"], ", ".join(res)))
                            invalid_tag = True

                    # Validate slugs
                    if "slug" in module.keys():
                        if not is_valid_slug(module["slug"]):
                            output.append("%s: invalid module slug" % (module["slug"]))
                            invalid_slug = True

                if invalid_tag:
                    output.append("-- Valid tags are defined in %s.\nSend an email to support@vcvrack.com to request new tags to be added to the database." % rack_tag_cpp_url)
                    failed = True

                if invalid_slug:
                    output.append("-- Valid slugs are defined in https://vcvrack.com/manual/PluginDevelopmentTutorial.html#naming")
                    failed = True

                # Validate URLs (if they exist; all URL keys are optional)
                for key in URL_KEYS:
                    if key in plugin_json.keys():
                        if plugin_json[key]:
                          if not is_valid_url(plugin_json[key]):
                                output.append("Invalid URL: %s" % plugin_json[key])
                                failed = True

                # Validate license is a valid SPDX ID
                valid = validate_license_id(valid_license_ids, plugin_json["license"])
                if not valid:
                    output.append("Invalid license ID: %s" % plugin_json["license"])
                    output.append("-- License must be a valid Identifier from https://spdx.org/licenses/")
                    failed = True

                # License can now be a URL, too. Validate it.
                if re.match("^https?\\:\\/\\/", plugin_json["license"]) is not None:
                    if not is_valid_url(plugin_json["license"]):
                        output.append("Invalid license URL: %s" % plugin_json["license"])
                        failed = True

                # Additional validations based on previous versions of the plugin.
                # Only applies if the repository is a submodule with a previously recorded SHA.
                if os.path.exists(os.path.join(os.path.dirname(plugin_root), ".gitmodules")):
                    submodule_sha = get_submodule_sha(plugin_root, plugin_path)
                    # If this is a new plugin or a newly migrated plugin, plugin.json does not exist in an old version
                    if submodule_sha:
                        head_sha = get_plugin_head_sha(plugin_path)

                        # Validate plugin version has been updated.
                        # If SHA of submodule is the same as HEAD, skip this. Version will be the same.
                        if submodule_sha != head_sha:
                            try:
                                old_version = get_plugin_version(plugin_path, submodule_sha)
                                new_version = get_plugin_version(plugin_path, head_sha)
                                if not Version(new_version) > Version(old_version):
                                    output.append("New version %s not greater than old version %s!" % (new_version, old_version))
                                    failed = True

                            except Exception as e:
                                print(e)
                                pass # Skip if no version available.

                        # Validate that plugin's slug has not changed.
                        (plugin_slug_changed, changed_plugin_slug) = check_for_plugin_slug_change(plugin_path, submodule_sha, head_sha)
                        if plugin_slug_changed:
                            output.append("Plugin slug change deteced: %s" % changed_plugin_slug)
                            failed = True

                        # Validate that plugin's module slugs have not changed or module was not removed.
                        (module_slug_changed, changed_module_slugs) = check_for_module_slug_changes(plugin_path, submodule_sha, head_sha)
                        if module_slug_changed:
                            output.append("Module slug changes detected: %s" % ", ".join(changed_module_slugs))
                            failed = True

            except FileNotFoundError as e:
                # No plugin.json to validate. Ignore.
                pass
            except json.decoder.JSONDecodeError as e:
                output.append("Invalid JSON format: %s" % e)
                failed = True
            except Exception as e:
                output.append("ERROR: %s" % e)
                failed = True

            if failed:
                print("FAILED")
                print("[%s] Issues found in `plugin.json`:\n" % plugin_name)
                print("\n".join(output))
                print()
            elif plugin_json == None:
                print("No plugin.json found")
            else:
                print("OK")

        return 1 if failed else 0

    except Exception as e:
        print("\nERROR: %s" % e)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
