//
//  testbundle.m
//  testbundle
//
//  Created by Stefano Cristiano on 01/10/22.
//

#import <XCTest/XCTest.h>
int main(int argc, const char* argv[]);

@interface testbundle : XCTestCase

@end

@implementation testbundle

- (void)setUp
{
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown
{
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)testMain
{
    // This is an example of a functional test case.
    // Use XCTAssert and related functions to verify your tests produce the correct results.
    main(0, 0);
}

//- (void)testPerformanceExample {
//    // This is an example of a performance test case.
//    [self measureBlock:^{
//        // Put the code you want to measure the time of here.
//    }];
//}

@end
