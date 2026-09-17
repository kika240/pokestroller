#import "session_store.h"
#include <CommonCrypto/CommonDigest.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static NSString *digest(NSData *data) {
    unsigned char bytes[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes,(CC_LONG)data.length,bytes);
    NSMutableString *hex=[NSMutableString string];
    for(size_t i=0;i<sizeof(bytes);i++) [hex appendFormat:@"%02x",bytes[i]];
    return hex;
}
static void storeError(NSError **error, NSString *message) {
    if(error) *error=[NSError errorWithDomain:@"PokeStroller.Session" code:1
        userInfo:@{NSLocalizedDescriptionKey:message}];
}
@implementation PWSessionStore {
    int _lockFD;
    NSData *_lastSaved;
}
+ (NSString *)identifierForROM:(NSData *)rom eeprom:(NSData *)eeprom profile:(NSString *)profile {
    NSMutableData *key=[NSMutableData dataWithData:[rom subdataWithRange:NSMakeRange(0,MIN(rom.length,49152))]];
    [key appendData:eeprom];
    [key appendData:[profile dataUsingEncoding:NSUTF8StringEncoding]];
    return digest(key);
}
- (instancetype)initWithRoot:(NSURL *)root identifier:(NSString *)identifier error:(NSError **)error {
    if(!(self=[super init])) return nil;
    _lockFD=-1;
    if(identifier.length!=64 || [identifier rangeOfCharacterFromSet:
        [[NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdef"] invertedSet]].location!=NSNotFound) {
        storeError(error,@"Identifiant de session invalide."); return nil;
    }
    _identifier=[identifier copy];
    _directory=[root URLByAppendingPathComponent:identifier isDirectory:YES];
    if(![NSFileManager.defaultManager createDirectoryAtURL:_directory withIntermediateDirectories:YES
        attributes:@{NSFilePosixPermissions:@0700} error:error]) return nil;
    _stateURL=[_directory URLByAppendingPathComponent:@"session.pws"];
    NSURL *lock=[_directory URLByAppendingPathComponent:@"session.lock"];
    _lockFD=open(lock.fileSystemRepresentation,O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
    if(_lockFD<0 || flock(_lockFD,LOCK_EX|LOCK_NB)) {
        storeError(error,@"Cette session est déjà ouverte dans une autre instance, ou son dossier est inaccessible. Utilisez un autre profil pour un second PokéWalker.");
        return nil;
    }
    return self;
}
- (void)dealloc { if(_lockFD>=0) close(_lockFD); }
- (NSData *)readState:(NSURL *)url {
    NSNumber *size=nil;
    [url getResourceValue:&size forKey:NSURLFileSizeKey error:NULL];
    if(!size || size.unsignedLongLongValue>131072) return nil;
    NSData *container=[NSData dataWithContentsOfURL:url];
    id state=container ? [NSPropertyListSerialization propertyListWithData:container
        options:NSPropertyListImmutable format:NULL error:NULL] : nil;
    if(![state isKindOfClass:NSDictionary.class]) return nil;
    NSData *bytes=state[@"eeprom"];
    if(![state[@"version"] isEqual:@1] || ![state[@"identity"] isEqual:self.identifier] ||
       ![bytes isKindOfClass:NSData.class] || bytes.length!=65536 ||
       ![state[@"sha256"] isEqual:digest(bytes)]) return nil;
    return bytes;
}
- (NSData *)restore:(NSError **)error {
    NSData *bytes=[self readState:self.stateURL];
    if(bytes){_lastSaved=bytes;return bytes;}
    NSURL *backup=[self.directory URLByAppendingPathComponent:@"previous.pws"];
    bytes=[self readState:backup];
    if(bytes){_lastSaved=nil;_recoveredBackup=YES;return bytes;}
    if([NSFileManager.defaultManager fileExistsAtPath:self.stateURL.path] ||
       [NSFileManager.defaultManager fileExistsAtPath:backup.path])
        storeError(error,@"La sauvegarde et sa copie précédente sont illisibles. Elles sont conservées ; choisissez un nouveau profil ou restaurez une exportation.");
    return nil;
}
- (BOOL)save:(NSData *)eeprom error:(NSError **)error {
    if(eeprom.length!=65536){storeError(error,@"Taille d’EEPROM invalide.");return NO;}
    if([eeprom isEqual:_lastSaved]) return YES;
    NSData *container=[NSPropertyListSerialization dataWithPropertyList:@{
        @"version":@1,@"identity":self.identifier,@"eeprom":eeprom,
        @"sha256":digest(eeprom),@"savedAt":NSDate.date
    } format:NSPropertyListBinaryFormat_v1_0 options:0 error:error];
    if(!container) return NO;
    if([self readState:self.stateURL]) {
        NSData *previous=[NSData dataWithContentsOfURL:self.stateURL options:0 error:error];
        if(!previous || ![previous writeToURL:[self.directory URLByAppendingPathComponent:@"previous.pws"]
            options:NSDataWritingAtomic error:error]) return NO;
    }
    if(![container writeToURL:self.stateURL options:NSDataWritingAtomic error:error]) return NO;
    // Atomic rename protects against partial writes; fsync requests persistence.
    int fd=open(self.stateURL.fileSystemRepresentation,O_RDONLY|O_CLOEXEC);
    if(fd<0 || fsync(fd)){if(fd>=0)close(fd);storeError(error,@"La sauvegarde n’a pas pu être synchronisée sur le disque.");return NO;}
    close(fd);_lastSaved=[eeprom copy];return YES;
}
@end
