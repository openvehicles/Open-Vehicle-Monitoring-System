//
//  ovmsControlCellularUsageViewController.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsControlCellularUsageViewController.h"

@implementation ovmsControlCellularUsageViewController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
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

/*
// Implement loadView to create a view hierarchy programmatically, without using a nib.
- (void)loadView
{
}
*/


// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
    [super viewDidLoad];

}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

-(void)viewDidAppear:(BOOL)animated
{  
  // Create barChart from theme
  barChart = [[CPTXYGraph alloc] initWithFrame:CGRectZero];
  CPTTheme *theme = [CPTTheme themeNamed:kCPTDarkGradientTheme];
  [barChart applyTheme:theme];
  CPTGraphHostingView *hostingView = (CPTGraphHostingView *)self.view;
  hostingView.hostedGraph = barChart;
  
  // Border
  barChart.plotAreaFrame.borderLineStyle = nil;
  barChart.plotAreaFrame.cornerRadius        = 0.0f;
  
  // Paddings
  barChart.paddingLeft   = 0.0f;
  barChart.paddingRight  = 0.0f;
  barChart.paddingTop        = 0.0f;
  barChart.paddingBottom = 0.0f;
  
  barChart.plotAreaFrame.paddingLeft       = 70.0;
  barChart.plotAreaFrame.paddingTop        = 20.0;
  barChart.plotAreaFrame.paddingRight      = 20.0;
  barChart.plotAreaFrame.paddingBottom = 80.0;
  
  // Graph title
  barChart.title = @"Graph Title\nLine 2";
  CPTMutableTextStyle *textStyle = [CPTTextStyle textStyle];
  textStyle.color                                   = [CPTColor grayColor];
  textStyle.fontSize                                = 16.0f;
  textStyle.textAlignment                   = CPTTextAlignmentCenter;
  barChart.titleTextStyle                   = textStyle;
  barChart.titleDisplacement                = CGPointMake(0.0f, -20.0f);
  barChart.titlePlotAreaFrameAnchor = CPTRectAnchorTop;
  
  // Add plot space for horizontal bar charts
  CPTXYPlotSpace *plotSpace = (CPTXYPlotSpace *)barChart.defaultPlotSpace;
  plotSpace.yRange = [CPTPlotRange plotRangeWithLocation:CPTDecimalFromFloat(0.0f) length:CPTDecimalFromFloat(300.0f)];
  plotSpace.xRange = [CPTPlotRange plotRangeWithLocation:CPTDecimalFromFloat(0.0f) length:CPTDecimalFromFloat(16.0f)];
  
  CPTXYAxisSet *axisSet = (CPTXYAxisSet *)barChart.axisSet;
  CPTXYAxis *x              = axisSet.xAxis;
  x.axisLineStyle                           = nil;
  x.majorTickLineStyle              = nil;
  x.minorTickLineStyle              = nil;
  x.majorIntervalLength             = CPTDecimalFromString(@"5");
  x.orthogonalCoordinateDecimal = CPTDecimalFromString(@"0");
  x.title                                           = @"X Axis";
  x.titleLocation                           = CPTDecimalFromFloat(7.5f);
  x.titleOffset                             = 55.0f;
  
  // Define some custom labels for the data elements
  x.labelRotation  = M_PI / 4;
  x.labelingPolicy = CPTAxisLabelingPolicyNone;
  NSArray *customTickLocations = [NSArray arrayWithObjects:[NSDecimalNumber numberWithInt:1], [NSDecimalNumber numberWithInt:5], [NSDecimalNumber numberWithInt:10], [NSDecimalNumber numberWithInt:15], nil];
  NSArray *xAxisLabels             = [NSArray arrayWithObjects:@"Label A", @"Label B", @"Label C", @"Label D", @"Label E", nil];
  NSUInteger labelLocation         = 0;
  NSMutableArray *customLabels = [NSMutableArray arrayWithCapacity:[xAxisLabels count]];
  for ( NSNumber *tickLocation in customTickLocations ) {
    CPTAxisLabel *newLabel = [[CPTAxisLabel alloc] initWithText:[xAxisLabels objectAtIndex:labelLocation++] textStyle:x.labelTextStyle];
    newLabel.tickLocation = [tickLocation decimalValue];
    newLabel.offset           = x.labelOffset + x.majorTickLength;
    newLabel.rotation         = M_PI / 4;
    [customLabels addObject:newLabel];
  }
  
  x.axisLabels = [NSSet setWithArray:customLabels];
  
  CPTXYAxis *y = axisSet.yAxis;
  y.axisLineStyle                           = nil;
  y.majorTickLineStyle              = nil;
  y.minorTickLineStyle              = nil;
  y.majorIntervalLength             = CPTDecimalFromString(@"50");
  y.orthogonalCoordinateDecimal = CPTDecimalFromString(@"0");
  y.title                                           = @"Y Axis";
  y.titleOffset                             = 45.0f;
  y.titleLocation                           = CPTDecimalFromFloat(150.0f);
  
  // First bar plot
  CPTBarPlot *barPlot = [CPTBarPlot tubularBarPlotWithColor:[CPTColor darkGrayColor] horizontalBars:NO];
  barPlot.baseValue  = CPTDecimalFromString(@"0");
  barPlot.dataSource = self;
  barPlot.barOffset  = CPTDecimalFromFloat(-0.25f);
  barPlot.identifier = @"Bar Plot 1";
  [barChart addPlot:barPlot toPlotSpace:plotSpace];
  
  // Second bar plot
  barPlot                                 = [CPTBarPlot tubularBarPlotWithColor:[CPTColor blueColor] horizontalBars:NO];
  barPlot.dataSource              = self;
  barPlot.baseValue               = CPTDecimalFromString(@"0");
  barPlot.barOffset               = CPTDecimalFromFloat(0.25f);
  barPlot.barCornerRadius = 2.0f;
  barPlot.identifier              = @"Bar Plot 2";
  [barChart addPlot:barPlot toPlotSpace:plotSpace];
}


-(NSUInteger)numberOfRecordsForPlot:(CPTPlot *)plot
{
  return 16;
}

-(NSNumber *)numberForPlot:(CPTPlot *)plot field:(NSUInteger)fieldEnum recordIndex:(NSUInteger)index
{
  NSDecimalNumber *num = nil;
  
  if ( [plot isKindOfClass:[CPTBarPlot class]] ) {
    switch ( fieldEnum ) {
      case CPTBarPlotFieldBarLocation:
        num = (NSDecimalNumber *)[NSDecimalNumber numberWithUnsignedInteger:index];
        break;
        
      case CPTBarPlotFieldBarTip:
        num = (NSDecimalNumber *)[NSDecimalNumber numberWithUnsignedInteger:(index + 1) * (index + 1)];
        if ( [plot.identifier isEqual:@"Bar Plot 2"] ) {
          num = [num decimalNumberBySubtracting:[NSDecimalNumber decimalNumberWithString:@"10"]];
        }
        break;
    }
  }
  
  return num;
}

/*
-(CPTFill *)barFillForBarPlot:(CPTBarPlot *)barPlot recordIndex:(NSNumber *)index;
{
  return nil;
}
*/
@end
