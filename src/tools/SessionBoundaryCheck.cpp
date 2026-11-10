#include "app/FeatureRunSession.h"
#include "app/RunSessionRegistry.h"
#include <type_traits>

// Compiled without Qt Widgets include paths/linkage. Reintroducing a widget
// into the session model breaks the normal app build.
#ifdef QT_WIDGETS_LIB
#error Session model must not depend on Qt Widgets
#endif
static_assert(std::is_same_v<decltype(FeatureRunSession::runningBlockHighlight), WorkflowExecutionHighlight>);
