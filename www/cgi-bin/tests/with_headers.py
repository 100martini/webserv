body = "<h1>Custom Headers Test</h1><p>Headers set correctly</p>"
print("Content-Type: text/html")
print(f"Content-Length: {len(body)}")
print()
print(body)