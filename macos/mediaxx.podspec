#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint mediaxx.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'mediaxx'
  s.version          = '0.0.1'
  s.summary          = 'Mediax analyse tools'
  s.description      = <<-DESC
Mediax analyse tools.
                       DESC
  s.homepage         = 'https://github.com/coolight7/mediaxx'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'coolight' => 'email@2465045051@qq.com' }

  # This will ensure the source files in Classes/ are included in the native
  # builds of apps using this FFI plugin. Podspec does not support relative
  # paths, so Classes contains a forwarder C file that relatively imports
  # `../src/*` so that the C sources can be shared among all target platforms.
  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'

  # If your plugin requires a privacy manifest, for example if it collects user
  # data, update the PrivacyInfo.xcprivacy file to describe your plugin's
  # privacy impact, and then uncomment this line. For more information,
  # see https://developer.apple.com/documentation/bundleresources/privacy_manifest_files
  # s.resource_bundles = {'mediaxx_privacy' => ['Resources/PrivacyInfo.xcprivacy']}

  s.dependency 'FlutterMacOS'

  s.platform = :osx, '11.0'
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES' }
  s.swift_version = '5.0'
end
