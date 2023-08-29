#if __APPLE__
#import "file_io.h"
#import <Foundation/Foundation.h>

@interface RppBundleLocator : NSObject
@end
@implementation RppBundleLocator
@end

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////

    static std::string to_string(NSString* str) noexcept
    {
        return str.length == 0 ? std::string{} : std::string{str.UTF8String}; // BUGFIX: don't use str.length (num unicode chars)
    }

    std::string apple_documents_dir() noexcept
    {
        NSArray<NSString*>* paths = NSSearchPathForDirectoriesInDomains(
                                        NSDocumentDirectory, NSUserDomainMask, YES);
        return to_string(paths[0]);
    }
    
    std::string apple_temp_dir() noexcept
    {
        return to_string(NSTemporaryDirectory());
    }

    std::string apple_module_dir(void* moduleObject) noexcept
    {
        Class clazz = RppBundleLocator.class;
        if (moduleObject)
            clazz = (Class)moduleObject;
        NSBundle* bundle = [NSBundle bundleForClass:clazz];
        return to_string(bundle.bundlePath) + "/";
    }

    std::string apple_module_path(void* moduleObject) noexcept
    {
        Class clazz = RppBundleLocator.class;
        if (moduleObject)
            clazz = (Class)moduleObject;
        NSBundle* bundle = [NSBundle bundleForClass:clazz];
        return to_string(bundle.executableURL.path);
    }

    ////////////////////////////////////////////////////////////////////////////////
}
#endif
