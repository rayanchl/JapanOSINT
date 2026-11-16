#!/usr/bin/env python3
"""Regenerate the small seed corpus under tests/fuzz/corpus/<target>/.

Seeds are real, tiny, well-formed documents (zip, xlsx, csv, html, rss/atom)
so libFuzzer starts from valid structure instead of rediscovering it. Run from
native/: python3 tests/fuzz/make_seeds.py
"""
import io, os, zipfile

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "corpus")

def w(target, name, data):
    d = os.path.join(root, target)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name), "wb") as f:
        f.write(data if isinstance(data, bytes) else data.encode())

def mkzip(entries, method=zipfile.ZIP_DEFLATED):
    b = io.BytesIO()
    with zipfile.ZipFile(b, "w", method) as z:
        for n, d in entries:
            z.writestr(n, d)
    return b.getvalue()

# zip
w("zipread", "deflate.zip", mkzip([("feed/stop_times.txt", "a,b,c\n1,2,3\n" * 20)]))
w("zipread", "stored.zip", mkzip([("a.txt", "hello"), ("b/c.txt", "world")], zipfile.ZIP_STORED))
w("zipread", "multi.zip", mkzip([("one.json", '{"id":1}'), ("two.json", '{"id":2}'), ("xl/workbook.xml", "<x/>")]))
w("zipread", "empty-member.zip", mkzip([("e.txt", "")]))

# xlsx
wb = ('<?xml version="1.0" encoding="UTF-8"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
      'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>'
      '<sheet name="S1" sheetId="1" r:id="rId1"/><sheet name="S2" sheetId="2" r:id="rId2"/></sheets></workbook>')
rels = ('<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" Type="x/worksheet" Target="worksheets/sheet1.xml"/>'
        '<Relationship Id="rId2" Type="x/worksheet" Target="worksheets/sheet2.xml"/></Relationships>')
sst = ('<?xml version="1.0"?><sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" count="3" uniqueCount="3">'
       '<si><t>name</t></si><si><t>東京</t></si><si><t>a &amp; b</t></si></sst>')
sheet = ('<?xml version="1.0"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>'
         '<row r="1"><c r="A1" t="s"><v>0</v></c><c r="B1" t="s"><v>1</v></c></row>'
         '<row r="2"><c r="A2"><v>42</v></c><c r="C2" t="s"><v>2</v></c><c r="D2" t="inlineStr"><is><t>x</t></is></c></row>'
         '</sheetData></worksheet>')
x = mkzip([("xl/workbook.xml", wb), ("xl/_rels/workbook.xml.rels", rels), ("xl/sharedStrings.xml", sst),
           ("xl/worksheets/sheet1.xml", sheet), ("xl/worksheets/sheet2.xml", sheet)])
w("xlsx", "two-sheets.xlsx", x)
w("xlsx", "not-a-workbook.zip", mkzip([("readme.txt", "x")]))

# csv
w("csv", "plain.csv", "id,name,city\n1,Alice,Tokyo\n2,Bob,Osaka\n")
w("csv", "quoted.csv", 'id,name\n1,"Smith, ""J"""\n2,"multi\nline"\n3,"unterminated\n')
w("csv", "pipe.csv", "# banner\nASN|name|ip\n1|a|1.2.3.4\n2|b|5.6.7.8\n")
w("csv", "dupe-headers.csv", "a,a,b,\n1,2,3,4\n,,,\n")
w("csv", "bom-crlf.csv", b"\xef\xbb\xbfid,\xe5\x90\x8d\xe5\x89\x8d\r\n1,\xe6\x9d\xb1\xe4\xba\xac\r\n")
w("csv", "tab.tsv", "a\tb\n1\t2\n")

# html
w("htmlparse", "list.html", '<html><body><h3>October</h3><a href="/a/1">Notice one</a><a href="/a/2?x=1&amp;y=2">Notice two</a>'
  '<a href="#top">top</a><a href="javascript:void(0)">js</a><a href="/p">»</a></body></html>')
w("htmlparse", "table.html", '<table><caption>Register</caption><thead><tr><th>Name</th><th colspan="2">Info</th></tr></thead>'
  '<tbody><tr><td rowspan="2"><a href="/x">A</a></td><td>1</td><td>2</td></tr><tr><td>3</td><td>4</td></tr></tbody></table>')
w("htmlparse", "messy.html", "<div><p>unclosed <b>bold <a href=x>link</div><script>if(a<b){}</script><!-- c --><a href='/q'>q</a")
w("htmlparse", "entities.html", '<a href="/e?a=1&amp;b=2&#38;c=3" title="&lt;t&gt;">&#26481;&#x4EAC; &nbsp; &copy;</a>')

# rss / atom
w("rss_atom", "rss2.xml", '<?xml version="1.0"?><rss version="2.0"><channel><title>T</title>'
  '<item><title>One</title><link>http://e/1</link><pubDate>Mon, 01 Nov 2026 00:00:00 GMT</pubDate><description><![CDATA[<p>x</p>]]></description></item>'
  '<item><title>Two &amp; three</title><link>http://e/2</link><guid>g2</guid></item></channel></rss>')
w("rss_atom", "atom.xml", '<?xml version="1.0"?><feed xmlns="http://www.w3.org/2005/Atom"><title>F</title>'
  '<entry><title>E1</title><link href="http://e/1" rel="alternate"/><updated>2026-11-01T00:00:00Z</updated><id>i1</id>'
  '<summary>s</summary></entry></feed>')
w("rss_atom", "rss1-rdf.xml", '<?xml version="1.0"?><rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#" '
  'xmlns="http://purl.org/rss/1.0/"><channel><items><rdf:Seq><rdf:li rdf:resource="http://e/1"/></rdf:Seq></items></channel>'
  '<item rdf:about="http://e/1"><title>R</title><link>http://e/1</link></item></rdf:RDF>')
w("rss_atom", "html-not-feed.html", "<html><head><title>Home</title></head><body>no feed here</body></html>")
w("rss_atom", "empty-feed.xml", '<?xml version="1.0"?><rss version="2.0"><channel><title>none</title></channel></rss>')
print("seeds written under", root)
