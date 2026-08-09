#!/bin/bash
echo "=== BOM rss index links"
curl -sL --max-time 25 http://www.bom.gov.au/rss/ | grep -oE 'href="[^"]*\.xml"' | sort -u | head -30
echo "=== vedur.is english earthquakes page rss links"
curl -sL --max-time 25 https://en.vedur.is/earthquakes-and-volcanism/earthquakes/ | grep -oiE '(href|src)="[^"]*(rss|xml|feed)[^"]*"' | sort -u | head -20
echo "=== emsc site rss links"
curl -sL --max-time 25 'https://www.emsc-csem.org/Earthquake_information/' | grep -oiE 'href="[^"]*(rss|feed|xml)[^"]*"' | sort -u | head -20
echo "=== copernicus activations rss links"
curl -sL --max-time 25 'https://mapping.emergency.copernicus.eu/activations/' | grep -oiE 'href="[^"]*(rss|feed|xml)[^"]*"' | sort -u | head -20
