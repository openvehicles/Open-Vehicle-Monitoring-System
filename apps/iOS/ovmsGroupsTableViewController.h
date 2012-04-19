//
//  ovmsGroupsTableViewController.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 16/4/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ovmsAppDelegate.h"

@interface ovmsGroupsTableViewController : UITableViewController <UITextFieldDelegate>
{
}

@property (strong, nonatomic) NSMutableArray* locationGroups;
@property (assign) int editing_insertrow;

- (IBAction)done:(id)sender;
- (IBAction)endedit:(id)sender;
- (IBAction)endswitchedit:(id)sender;

@end
