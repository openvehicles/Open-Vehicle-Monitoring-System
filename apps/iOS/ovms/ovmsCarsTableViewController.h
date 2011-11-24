//
//  ovmsCarsTableViewController.h
//  ovms
//
//  Created by Mark Webb-Johnson on 23/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ovmsAppDelegate.h"

@interface ovmsCarsTableViewController : UITableViewController
{
  NSArray *_cars;
  NSManagedObjectContext *_context;
}

@property (nonatomic, retain) NSArray *cars;
@property (nonatomic, retain) NSManagedObjectContext *context;

@end
