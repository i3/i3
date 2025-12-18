#!/bin/bash
# AI Suggest - stores suggestion for bar display and user acceptance
# Creates a pending suggestion file that ai3-status reads

SUGGESTION_FILE="/tmp/ai3-suggestion.json"

# Clear any existing suggestion
rm -f "$SUGGESTION_FILE"

# Get suggestion from server and store it
curl -s -X POST http://127.0.0.1:7878/suggest \
    -H "Content-Type: application/json" \
    -d '{"include_screenshot": false}' \
    | python3 -c "
import sys, json
data = json.load(sys.stdin)
if data.get('suggestion'):
    s = data['suggestion']
    # Write simplified suggestion to file
    output = {
        'action': s.get('action_type', 'unknown'),
        'description': s.get('description', '')[:60],
        'keys': s.get('keys', ''),
        'command': s.get('command', ''),
        'confidence': s.get('confidence', 0),
        'target_id': s.get('target_id'),
        'full': s
    }
    with open('/tmp/ai3-suggestion.json', 'w') as f:
        json.dump(output, f)
    print('Suggestion ready - see bar')
else:
    print('No suggestion available')
"
