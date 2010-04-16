#ifndef _MATCH_H
#define _MATCH_H

bool match_is_empty(Match *match);
bool match_matches_window(Match *match, i3Window *window);

#endif
