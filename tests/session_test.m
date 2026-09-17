#import "session_store.h"
#include <assert.h>
int main(int argc,char **argv) {
    assert(argc==2);
    @autoreleasepool {
        NSURL *root=[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[1]] isDirectory:YES];
        NSMutableData *rom=[NSMutableData dataWithLength:49152],*ee=[NSMutableData dataWithLength:65536];
        NSString *key=[PWSessionStore identifierForROM:rom eeprom:ee profile:@"test"];
        assert(![key isEqual:[PWSessionStore identifierForROM:rom eeprom:ee profile:@"second"]]);
        NSError *error=nil;
        PWSessionStore *store=[[PWSessionStore alloc] initWithRoot:root identifier:key error:&error];
        assert(store && !error && ![store restore:&error] && !error);
        PWSessionStore *duplicate=[[PWSessionStore alloc] initWithRoot:root identifier:key error:&error];
        assert(!duplicate && error);error=nil;
        assert([store save:ee error:&error]);
        ((uint8_t*)ee.mutableBytes)[123]=42;
        assert([store save:ee error:&error]);
        NSURL *url=store.stateURL;
        store=nil;
        store=[[PWSessionStore alloc] initWithRoot:root identifier:key error:&error];
        assert([[store restore:&error] isEqual:ee]);
        assert([[@"corrupt" dataUsingEncoding:NSUTF8StringEncoding] writeToURL:url atomically:YES]);
        store=nil;
        store=[[PWSessionStore alloc] initWithRoot:root identifier:key error:&error];
        NSData *backup=[store restore:&error];
        assert(backup && ((const uint8_t*)backup.bytes)[123]==0 && store.recoveredBackup);
        assert([store save:backup error:&error]);assert([[store restore:&error] isEqual:backup]);
        assert(![store save:[NSData data] error:&error]);
        puts("PASS autosave/restart, profile isolation, exclusive lock, corruption recovery");
    }
}
