/*
 * Lets clicks fall through decorative content to the widget underneath it
 */

#ifndef AMETHYST__CLICK_THROUGH_H
#define AMETHYST__CLICK_THROUGH_H

namespace Amethyst {

class Instance;

/**
 * @brief Makes passive content (plain frames, containers, labels) pass clicks on instead of consuming them, so a press
 * on a cell's text still reaches the row behind it. Walks down through passive nodes only: anything interactive keeps
 * its own click handling, and so does everything inside it.
 * @param root The content to walk, typically one table or tree cell
 */
void passClicksThroughPassiveContent(Instance *root);

} // namespace Amethyst

#endif // AMETHYST__CLICK_THROUGH_H
