import sys
import urllib.parse

data = sys.stdin.read()
params = urllib.parse.parse_qs(data)
name = params.get("name", ["stranger"])[0]

print("Content-Type: text/html")
print()
print(f"<h1>Hi, {name}!</h1>")
