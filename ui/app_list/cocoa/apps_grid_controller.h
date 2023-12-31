// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_
#define UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "ui/app_list/cocoa/scroll_view_with_no_scrollbars.h"

namespace app_list {
class AppListModel;
class AppListViewDelegate;
class AppsGridDelegateBridge;
}

@class AppsGridViewItem;
@protocol AppsPaginationModelObserver;

// Controls a grid of views, representing AppListModel::Apps sub models.
@interface AppsGridController : NSViewController<GestureScrollDelegate> {
 @private
  scoped_ptr<app_list::AppListModel> model_;
  app_list::AppListViewDelegate* delegate_;  // Weak. Owned by view controller.
  scoped_ptr<app_list::AppsGridDelegateBridge> bridge_;

  scoped_nsobject<NSMutableArray> pages_;
  scoped_nsobject<NSMutableArray> items_;

  id<AppsPaginationModelObserver> paginationObserver_;

  // Index of the currently visible page.
  size_t visiblePage_;

  // Whether we are currently animating a scroll to the nearest page.
  BOOL animatingScroll_;
}

@property(assign, nonatomic) id<AppsPaginationModelObserver> paginationObserver;

+ (void)setScrollAnimationDuration:(NSTimeInterval)duration;

- (NSCollectionView*)collectionViewAtPageIndex:(size_t)pageIndex;

- (AppsGridViewItem*)itemAtIndex:(size_t)itemIndex;

- (app_list::AppListModel*)model;

- (void)setModel:(scoped_ptr<app_list::AppListModel>)newModel;

- (void)setDelegate:(app_list::AppListViewDelegate*)newDelegate;

- (size_t)visiblePage;

// Calls delegate_->ActivateAppListItem for the currently selected item by
// simulating a click.
- (void)activateSelection;

// Return the number of pages of icons in the grid.
- (size_t)pageCount;

// Scroll to a page in the grid view with an animation.
- (void)scrollToPage:(size_t)pageIndex;

@end

#endif  // UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_
