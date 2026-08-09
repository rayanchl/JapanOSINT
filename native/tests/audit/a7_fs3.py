import re
s = open('/tmp/a7x.xml', encoding='utf-8', errors='replace').read()
i = s.lower().find('<item')
print(repr(s[i:i + 1500]))
print("=== self-closing count:", len(re.findall(r'<item\b[^>]*/>', s, re.I)))
print("=== paired count:", len(re.findall(r'<item\b[^>]*>.*?</item>', s, re.I | re.S)))
