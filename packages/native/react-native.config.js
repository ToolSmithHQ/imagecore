module.exports = {
  dependency: {
    platforms: {
      ios: {
        podspecPath: __dirname + '/ios/ImageCore.podspec',
      },
      android: {
        sourceDir: __dirname + '/android',
        packageImportPath: 'import com.toolsmith.imagecore.ImageCorePackage;',
        packageInstance: 'new ImageCorePackage()',
      },
    },
  },
};
