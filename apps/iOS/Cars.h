//
//  Cars.h
//  ovms
//
//  Created by Mark Webb-Johnson on 24/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>


@interface Cars : NSManagedObject

@property (nonatomic, retain) NSString * vehicleid;
@property (nonatomic, retain) NSString * label;
@property (nonatomic, retain) NSString * imagepath;
@property (nonatomic, retain) NSString * netpass;
@property (nonatomic, retain) NSString * userpass;

@end
