//
//  ovmsCarsTableViewController.m
//  ovms
//
//  Created by Mark Webb-Johnson on 23/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "ovmsCarsTableViewController.h"
#import "ovmsCarsFormViewController.h"
#import "Cars.h"

@implementation ovmsCarsTableViewController

@synthesize cars = _cars;
@synthesize context = _context;

- (id)initWithStyle:(UITableViewStyle)style
{
    self = [super initWithStyle:style];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void)didReceiveMemoryWarning
{
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc that aren't in use.
}

#pragma mark - View lifecycle

- (void)viewDidLoad
{
    [super viewDidLoad];

    // Uncomment the following line to preserve selection between presentations.
    self.clearsSelectionOnViewWillAppear = NO;
 
    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    self.navigationItem.rightBarButtonItem = self.editButtonItem;
  
  self.title = @"Cars";
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];

  int originalcount = [_cars count];
  
  _context = [ovmsAppDelegate myRef].managedObjectContext;
  NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
  NSEntityDescription *entity = [NSEntityDescription 
                                 entityForName:@"Cars" inManagedObjectContext:_context];
  [fetchRequest setEntity:entity];
  NSError *error;
  self.cars = [_context executeFetchRequest:fetchRequest error:&error];

  if (originalcount>0)
    {
    [self.tableView reloadData];
    }

  for (int k=0;k<[_cars count]; k++)
    {
    Cars *car = [_cars objectAtIndex:k];
    if ([car.vehicleid isEqualToString:[ovmsAppDelegate myRef].sel_car])
      {
      NSIndexPath *indexPath = [NSIndexPath indexPathForRow: k inSection: 0];
      [self.tableView selectRowAtIndexPath:indexPath animated:NO scrollPosition:UITableViewScrollPositionMiddle];
      }
    }
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
  if ([[segue identifier] isEqualToString:@"editCar"])
    {
    NSIndexPath *indexPath = [self.tableView indexPathForSelectedRow];
    Cars *car = [_cars objectAtIndex:indexPath.row];
    [[segue destinationViewController] setCarEditing:car.vehicleid];
    }
  else if ([[segue identifier] isEqualToString:@"newCar"])
    {
    [[segue destinationViewController] setCarEditing:nil];
    }
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    // Return the number of sections.
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    // Return the number of rows in the section.
   return [_cars count];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *CellIdentifier = @"CellIdentifier";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier];
    }

  // Retrieve the relevant car record
  Cars *car = [_cars objectAtIndex:indexPath.row];

  // Get the cell label using its tag and set it
  UILabel *cellLabel = (UILabel *)[cell viewWithTag:1];
  [cellLabel setText:car.label];
  
  // get the cell imageview using its tag and set it
  UIImageView *cellImage = (UIImageView *)[cell viewWithTag:2];
  [cellImage setImage:[UIImage imageNamed:[NSString stringWithFormat:car.imagepath, indexPath.row]]];
  
  return cell;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath {
  return 144;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (editingStyle == UITableViewCellEditingStyleDelete)
      {
      // Delete the row from the data source
      if ([_cars count]==1) return; // Can't delete the last car
      Cars *car = [_cars objectAtIndex:indexPath.row];
      [_context deleteObject:car];
      NSError *error;
      if (![_context save:&error])
        {
        NSLog(@"Whoops, couldn't save: %@", [error localizedDescription]);
        return;
        }

      // Reload the cars array...
      NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
      NSEntityDescription *entity = [NSEntityDescription 
                                     entityForName:@"Cars" inManagedObjectContext:_context];
      [fetchRequest setEntity:entity];
      self.cars = [_context executeFetchRequest:fetchRequest error:&error];

      [tableView deleteRowsAtIndexPaths:[NSArray arrayWithObject:indexPath] withRowAnimation:UITableViewRowAnimationFade];
      }   
    else if (editingStyle == UITableViewCellEditingStyleInsert)
      {
        // Create a new instance of the appropriate class, insert it into the array, and add a new row to the table view
      }   
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  Cars* car = [_cars objectAtIndex:indexPath.row];
  if (! [[ovmsAppDelegate myRef].sel_car isEqualToString:car.vehicleid])
    {
    // Switch the car
    [[ovmsAppDelegate myRef] switchCar:car.vehicleid];
    }
}

@end
