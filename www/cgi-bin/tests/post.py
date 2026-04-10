import os
import sys

print("Content-Type: text/plain")
print()

content_length = os.environ.get('CONTENT_LENGTH', 0)
if content_length:
    body = sys.stdin.read(int(content_length))
    print(f"Received: {body}")
else:
    print("No POST data")