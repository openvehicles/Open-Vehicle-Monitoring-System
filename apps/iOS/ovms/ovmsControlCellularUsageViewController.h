//
//  ovmsControlCellularUsageViewController.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "CorePlot-CocoaTouch.h"

@interface ovmsControlCellularUsageViewController : UIViewController<CPTPlotDataSource>
{
  CPTXYGraph *barChart;
}

@end
