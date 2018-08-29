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

    static string to_string(NSString* str) noexcept
    {
        return str.length == 0 ? string{} : string{str.UTF8String}; // BUGFIX: don't use str.length (num unicode chars)
    }

    string apple_documents_dir() noexcept
    {
        NSArray<NSString*>* paths = NSSearchPathForDirectoriesInDomains(
                                        NSDocumentDirectory, NSUserDomainMask, YES);
        return to_string(paths[0]);
    }
    
    string apple_temp_dir() noexcept
    {
        return to_string(NSTemporaryDirectory());
    }

    string apple_module_dir(void* moduleObject) noexcept
    {
        NSBundle* bundle = [NSBundle bundleForClass:RppBundleLocator.class];
        return to_string(bundle.bundlePath) + "/";
    }

    ////////////////////////////////////////////////////////////////////////////////
}
#endif
