#!/usr/bin/env bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
IDS='ahram-eg|dhaka-tribune|eluniversal-mx|emol-cl|gulf-news|haaretz-2|infobae|jakarta-post|manila-times|milenio|nation-ke|news24-za|pravda-ua|presstv|rt-news|the-national-ae|hurriyet-en|ap-topnews|mail-guardian-africa|packetstorm|talos-disclosures|tenable-tra|al-jazeera-ar|folha-br|korea-times'
grep -nE "\"($IDS)\"" -A2 collectors/sources/world_outlets_1.c collectors/sources/intel_worldnews.c collectors/sources/intel_vuln_world.c
