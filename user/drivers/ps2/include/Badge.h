#ifndef BADGE_H
#define BADGE_H

/**
 * Used to restrict access to public functions to only a particular class.
 */
template<typename T>
class Badge {
    friend T;
    Badge() {}
};

#endif
