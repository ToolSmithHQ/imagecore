/**
 * Expo config plugin for @toolsmithhq/imagecore-native.
 * Registers ImageCorePackage in Android MainApplication.
 */

let configPlugins;
try {
  configPlugins = require("expo/config-plugins");
} catch {
  // When running from symlinked package, resolve from the app's node_modules
  configPlugins = require(require.resolve("expo/config-plugins", {
    paths: [process.cwd()],
  }));
}

const { withMainApplication, createRunOncePlugin } = configPlugins;

const PACKAGE_NAME = "@toolsmithhq/imagecore-native";
const PACKAGE_VERSION = "0.1.0";

function withImageCoreAndroid(config) {
  return withMainApplication(config, (config) => {
    let contents = config.modResults.contents;

    if (!contents.includes("ImageCorePackage")) {
      // Add import
      contents = contents.replace(
        /(import com\.facebook\.react\.)/,
        "import com.toolsmith.imagecore.ImageCorePackage\n$1"
      );

      // Add to getPackages() .apply { } block
      contents = contents.replace(
        /(PackageList\(this\)\.packages\.apply\s*\{)/,
        "$1\n              add(ImageCorePackage())"
      );
    }

    config.modResults.contents = contents;
    return config;
  });
}

function withImageCore(config) {
  config = withImageCoreAndroid(config);
  return config;
}

module.exports = createRunOncePlugin(
  withImageCore,
  PACKAGE_NAME,
  PACKAGE_VERSION
);
