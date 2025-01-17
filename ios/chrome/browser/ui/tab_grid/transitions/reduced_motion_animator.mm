// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/transitions/reduced_motion_animator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ReducedMotionAnimator

@synthesize presenting = _presenting;

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0.25;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // Get views and view controllers for this transition.
  UIView* containerView = [transitionContext containerView];

  UIViewController* appearingViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  // The disappearing view, already in the view hierarchy.
  UIView* disappearingView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];
  // The appearing view, not yet in the view hierarchy.
  UIView* appearingView =
      [transitionContext viewForKey:UITransitionContextToViewKey];

  // Add the tab view to the container view with the correct size.
  appearingView.frame =
      [transitionContext finalFrameForViewController:appearingViewController];
  // If presenting, the appearing view does in front of the disappearing view.
  // If dismissing, the disappearing view stays in front.
  if (self.presenting) {
    [containerView addSubview:appearingView];
  } else {
    [containerView insertSubview:appearingView belowSubview:disappearingView];
  }

  // The animation here creates a simple quick zoom effect -- the tab view
  // fades in/out as it expands/contracts. The zoom is not large (80% to 100%)
  // and is centered on the view's final center position, so it's not directly
  // connected to any tab grid positions.
  UIView* animatingView;
  CGFloat finalAnimatingViewAlpha;
  CGAffineTransform finalAnimatingViewTransform;

  if (self.presenting) {
    // If presenting, the appearing view (the tab view) animates in from 0%
    // opacity and an 80% scale transform.
    animatingView = appearingView;
    finalAnimatingViewAlpha = animatingView.alpha;
    animatingView.alpha = 0;
    finalAnimatingViewTransform = animatingView.transform;
    animatingView.transform =
        CGAffineTransformScale(finalAnimatingViewTransform, 0.8, 0.8);
  } else {
    // If dismissing, the disappearing view (the tab view) animates out
    // to 0% opacity and 80% scale.
    animatingView = disappearingView;
    finalAnimatingViewAlpha = 0;
    finalAnimatingViewTransform =
        CGAffineTransformScale(animatingView.transform, 0.8, 0.8);
  }

  // Animate the animating view to final properties, then clean up by removing
  // the disappearing view.
  [UIView animateWithDuration:[self transitionDuration:transitionContext]
      delay:0.0
      options:UIViewAnimationOptionCurveEaseOut
      animations:^{
        animatingView.alpha = finalAnimatingViewAlpha;
        animatingView.transform = finalAnimatingViewTransform;
      }
      completion:^(BOOL finished) {
        // If the transition was cancelled, remove the disappearing view.
        // If not, remove the appearing view.
        if (transitionContext.transitionWasCancelled) {
          [appearingView removeFromSuperview];
        } else {
          [disappearingView removeFromSuperview];
        }
        // Mark the transition as completed.
        [transitionContext completeTransition:YES];
      }];
}

@end
