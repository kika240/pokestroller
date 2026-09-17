#import <Foundation/Foundation.h>

// One writer per ROM/initial-EEPROM/profile, outside the checkout. No ROM saved.
@interface PWSessionStore : NSObject
@property(nonatomic, readonly) NSURL *directory;
@property(nonatomic, readonly) NSURL *stateURL;
@property(nonatomic, readonly) NSString *identifier;
@property(nonatomic, readonly) BOOL recoveredBackup;
+ (NSString *)identifierForROM:(NSData *)rom eeprom:(NSData *)eeprom profile:(NSString *)profile;
- (instancetype)initWithRoot:(NSURL *)root identifier:(NSString *)identifier error:(NSError **)error;
- (NSData *)restore:(NSError **)error;
- (BOOL)save:(NSData *)eeprom error:(NSError **)error;
@end
