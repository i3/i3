#!/bin/bash
# Discard pending AI suggestion

SUGGESTION_FILE="/tmp/ai3-suggestion.json"
rm -f "$SUGGESTION_FILE"
echo "Suggestion discarded"
