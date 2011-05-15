/*
 * vim:ts=4:sw=4:expandtab
 *
 */
#ifndef _ASSIGNMENTS_H
#define _ASSIGNMENTS_H

/**
 * Checks the list of assignments for the given window and runs all matching
 * ones (unless they have already been run for this specific window).
 *
 */
void run_assignments(i3Window *window);

#endif
