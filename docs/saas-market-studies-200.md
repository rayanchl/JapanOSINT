# 200 SaaS concepts across OSINT, cyber, health and AI — with market studies

_Research date: 2026-08-10 · all figures retrieved 2026-08-10 · author: Claude (Opus 5)_

---

## 0. How to read this, and what is and isn't sourced

This document contains 200 product concepts and a market study for each. It was
written under the same rule the rest of this repo runs on: **what is displayed
is exactly what was actually obtained.** That rule cuts hard here, because the
honest version of "200 market studies" is not 200 independent primary research
projects. It is this:

**What is real and sourced.** 49 market anchors (§1) were retrieved by live web
search on 2026-08-10. Every one carries a figure, the firm that published it,
and a URL. Every one of the 200 studies names the anchor it rests on. Where two
firms disagree — and they disagree constantly — both numbers are shown.

**What is my derivation, labelled as such.** Each study's SAM line is
arithmetic I performed on an anchor, with the assumption stated inline. A SAM
line is never a sourced fact and is never presented as one. Where I write
"assume", that is me, not a research firm.

**What is my judgement.** Wedge, pricing, risk and verdict are analysis. They
are falsifiable opinions, not findings.

**What is *not* here, and you should know it.** No study rests on primary
customer interviews, because none were conducted. No study contains a revenue
figure for a private competitor unless a real funding or share datapoint turned
up in search. No entry invents a market number when the search didn't produce
one — those entries say `no clean anchor found` and are scored down for it.

### The number that should worry you most

The single most important finding of this research is that **the market-sizing
industry does not agree with itself.** Real examples from §1, all retrieved the
same day:

| Market | Low estimate (2026) | High estimate (2026) | Spread |
|---|---|---|---|
| OSINT | $5.22B | $22.35B | **4.3×** |
| Geospatial imagery analytics | $16.19B | $36.01B | 2.2× |
| Digital risk protection | $1.20B (2025) | $96.98B | **80×** |
| AI in drug discovery | $3.5B | $24.51B | **7×** |
| AI in medical imaging | $2.16B | $3.8B | 1.8× |
| Healthcare cybersecurity | $28.4B | $42.31B | 1.5× |
| Femtech | $9.78B | $55.88B | **5.7×** |

An 80× spread on "digital risk protection" is not a rounding difference; it
means at least one of those firms is measuring something entirely different
from the other and both are selling the report. **Treat every TAM in this
document as a directional claim about which markets are big relative to each
other, and none of them as a number you would put in a board deck without
buying the underlying report and reading its segmentation.** Where the spread
exceeds 3×, the study says so.

### Geographic framing

The concepts are written **global-first with a Japan wedge called out where one
genuinely exists** — this repo is Japan-focused, and three of the strongest
regulatory catalysts found in research (the Active Cyber Defense Law taking
effect 2026-10-01, the April 2026 APPI amendment introducing administrative
fines, and the Medical DX nationwide EMR rollout) are Japanese. **56 of the 200
studies cite a Japan-only anchor** (`A4`, `A7`, `A11`, `A20`, `A24`, `A39`,
`A43`); of those, roughly 40 are concepts that only make sense in Japan. The
rest are global concepts where Japan is a market, not the market.

### Scoring

Each study ends with three 1–5 scores and a call.

- **Pull** — how hard the market is already pulling. 5 = a regulation with a
  date on it forces a purchase. 1 = you must create the category.
- **Moat** — what stops a funded incumbent copying it in two quarters.
  5 = proprietary data or regulatory lock-in. 1 = a prompt and a good UI.
- **TTR** — time to first revenue. 5 = under 3 months. 1 = 2+ years of
  clinical or regulatory work first.

**Calls:** `BUILD` (38), `WEDGE` (73 — real but needs a specific unfair
advantage), `WATCH` (64 — market not ready or too crowded today), `AVOID`
(25 — structurally bad, and the study says why).

The 25 `AVOID` entries are deliberate. A list of 200 ideas where all 200 are
good is a list that has not been thought about. Some of them (#022 pre-hire
screening, #171 AI-text detection in education) are rejected on ethical
grounds and say so; they are included because a survey that silently drops
what it considered is not a survey.

---

## 1. Market anchors — the sourced base

Every study below cites one of these by ID. Retrieved 2026-08-10.

| ID | Market | Figure | Source |
|---|---|---|---|
| **A1** | OSINT | $21.06B (2026), 15.62% CAGR · also $22.35B→$75.60B by 2035 @14.5% · also $14.16B · also $5.22B **(4.3× spread)** | [Mordor](https://www.mordorintelligence.com/industry-reports/open-source-intelligence-market), [MRFR](https://www.marketresearchfuture.com/reports/open-source-intelligence-market-4545), [GMI](https://www.gminsights.com/industry-analysis/open-source-intelligence-osint-market) |
| **A2** | Threat intelligence | $11.55B (2025) → $22.97B (2030) @14.7% · TIP subsegment $13.56B (2025) → $36.53B (2030) @20.15% | [M&M](https://www.marketsandmarkets.com/PressReleases/threat-intelligence-security.asp), [Mordor](https://www.mordorintelligence.com/industry-reports/threat-intelligence-platforms-market) |
| **A3** | Digital health | $491.62B (2026) @21.6% · also $420.2B @23.4% | [Fortune BI](https://www.fortunebusinessinsights.com/industry-reports/digital-health-market-100227), [Grand View](https://www.grandviewresearch.com/industry-analysis/digital-health-market) |
| **A4** | Japan cybersecurity | $11.36B (2026) → $18.76B (2031) @10.57% | [Mordor](https://www.mordorintelligence.com/industry-reports/japan-cybersecurity-market) |
| **A5** | Attack surface management | $1.65B–$2.03B (2026), up to 31.3% CAGR → $5B (2034). Vendors: Microsoft, Tenable, CrowdStrike, Rapid7, Recorded Future, Bitsight, Armis, SecurityScorecard, UpGuard, Cymulate, Panorays, CybelAngel, CyCognito, Edgescan | [Fortune BI](https://www.fortunebusinessinsights.com/attack-surface-management-market-110386), [360i](https://www.360iresearch.com/library/intelligence/attack-surface-management) |
| **A6** | Healthcare cybersecurity | $42.31B (2026) → $97.79B (2031) · also $33.16B @20.7% · medical device security $8.05B–$8.30B (2026) → $22.69B (2034) @13.39% | [Mordor](https://www.mordorintelligence.com/industry-reports/medical-device-cybersecurity-market), [TBRC](https://www.thebusinessresearchcompany.com/report/healthcare-cyber-security-global-market-report), [Fortune BI](https://www.fortunebusinessinsights.com/medical-device-security-market-115040) |
| **A7** | Japan digital health | $29.2B (2024) → $55.8B (2033) @7.5% · Japan EMR $494.83M (2024) → $853.55M (2033) @6.3% | [IMARC](https://www.imarcgroup.com/japan-digital-health-market), [Spherical](https://www.sphericalinsights.com/reports/japan-electronic-health-records-market) |
| **A8** | Third-party risk mgmt | $10.60B (2026) → $20.71B (2031) @14.34% · third-party breaches **+54% 2020–2024** · 82% of enterprises use external vendors | [Mordor](https://www.mordorintelligence.com/industry-reports/third-party-risk-management-market), [DataM](https://www.openpr.com/news/4525973/third-party-risk-management-market-to-reach-us-21-97-billion) |
| **A9** | Ambient AI scribe | $1.75B (2025) · genAI clinical documentation $1.05B (2026) → $10.50B (2034) @33.3% · US medical scribing $621.66M (2026) → $4.67B (2035) @25.12% | [Fortune BI](https://www.fortunebusinessinsights.com/generative-ai-for-clinical-documentation-market-115967), [Astute](https://www.astuteanalytica.com/industry-report/ai-clinical-documentation-ambient-scribe-market) |
| **A10** | Dark web monitoring | $1.2B (2025) → $4.1B (2034) @14.6% · **6.8B stolen credentials** traded 2025 (+22% YoY) · 73% of breached creds appear within 48h | [Dataintelo](https://dataintelo.com/report/dark-web-monitoring-market) |
| **A11** | 🇯🇵 Active Cyber Defense Law | Enacted **2025-05-16**; most provisions effective **2026-10-01**; full effect 2027. 15 critical-infrastructure sectors. Mandatory incident reporting + "Critical System" notification | [Baker McKenzie](https://connectontech.bakermckenzie.com/japans-new-active-cyber-defense-law-impact-on-businesses/), [Nippon.com](https://www.nippon.com/en/in-depth/d01147/) |
| **A12** | AI governance | $227–340M (2024–25) → $4.83B (2034) @35–45% · 54% of IT leaders rank it a core concern (was 29% in 2024) · **only 6% have advanced AI security strategies** | [Modulos](https://www.modulos.ai/best-ai-governance-platforms/), [Maxim](https://www.getmaxim.ai/articles/top-5-ai-guardrails-platforms-for-responsible-enterprise-ai-in-2026/) |
| **A13** | Remote patient monitoring | $30.9B–$67.3B (2026) **(2.2× spread)** · chronic disease mgmt = 35.6% of services (2025) | [Grand View](https://www.grandviewresearch.com/industry-analysis/remote-patient-monitoring-devices-market), [R&M](https://www.researchandmarkets.com/reports/6226093/remote-patient-monitoring-market-report) |
| **A14** | Deepfake / synthetic media | Detection ~$15.7B by 2026 @~42% · voice cloning $4.06B (2026) @23.9% · **8M deepfakes** circulating (vs 500k in 2023) · 1 in 5 biometric fraud attempts · vishing **+1,633%** Q1 2025 | [Biometric Update](https://www.biometricupdate.com/202607/the-deepfake-fraud-detection-market-2026-securing-identity-in-the-ai-era), [StationX](https://app.stationx.net/articles/deepfake-statistics) |
| **A15** | OT / ICS security | ICS $20.55B (2026) · OT $27.39B (2026) → $58.94B (2031) @16.6% | [M&M](https://www.marketsandmarkets.com/Market-Reports/operational-technology-ot-security-market-18524133.html) |
| **A16** | Digital therapeutics | $12.45B (2026) → $67.58B (2034) · mental-health DTx $4.51B (2026) → $24.42B (2035) @20.64% · **CMS began reimbursing FDA-authorized digital mental health treatments 2025-01-01** | [Towards Healthcare](https://www.towardshealthcare.com/insights/digital-therapeutics-for-mental-health-market-sizing), [Global Growth](https://www.globalgrowthinsights.com/market-reports/digital-therapeutics-market-107083) |
| **A17** | AI medical imaging | $2.16B (2026) → $8.23B (2031) @30.7% · also $3.8B @31.2% · AI radiology $600.8M (2026) → $3.23B (2034) @23.38% | [Mordor](https://www.mordorintelligence.com/industry-reports/ai-market-in-medical-imaging), [Fortune BI](https://www.fortunebusinessinsights.com/ai-in-radiology-market-115732) |
| **A18** | Software supply chain / SBOM | SSCS $2.16B (2026) → $3.12B (2034) @4.72% · SBOM mgmt & compliance → $9.7B (2035) @13.2% | [VMR](https://www.verifiedmarketreports.com/product/software-supply-chain-security-market/), [FMI](https://www.futuremarketinsights.com/reports/sbom-management-and-software-supply-chain-compliance-market) |
| **A19** | AML / KYC | AML software $3.84B (2025) → $10.74B (2035) · AML+KYC → $5.36B by 2027 @17.3% | [Precedence](https://www.precedenceresearch.com/anti-money-laundering-software-market) |
| **A20** | 🇯🇵 Elderly care tech | $1.18B (2025) → $3.76B (2033) @15.59% · services $11.77B (2024) @7.49% · **2.4M caregivers needed FY2026, 250k short; 570k short by FY2040** · job-to-applicant ratio **3.6** | [DataM](https://www.openpr.com/news/4512799/japan-elderly-care-technology-market-to-add-us-2-58-billion), [TechSci](https://www.techsciresearch.com/news/19664-japan-elderly-care-services-market.html) |
| **A21** | Cyber insurance | $23.29B (2026) · $22.88B → $44.67B (2032) · Zurich took a minority stake in Safe Security to feed CRQ into underwriting | [Mordor](https://www.mordorintelligence.com/industry-reports/cybersecurity-insurance-market), [R&M](https://www.researchandmarkets.com/report/cyberinsurance) |
| **A22** | Decentralized clinical trials | $10.74B–$14.29B (2026) @14.42% · Medidata >13% share; top-5 (Medidata, IQVIA, ICON, Parexel, Fortrea) = **46%** | [Mordor](https://www.mordorintelligence.com/industry-reports/decentralized-clinical-trials-market) |
| **A23** | Digital risk protection | $1.20B (2025) → $5.51B (2034) @18.5% · **also quoted at $96.98B (2026) — an 80× definitional spread** · anti-phishing → $10.88B (2034) @13.8% | [TrendX](https://trendxinsights.com/syndicated-market-research-reports/digital-risk-protection-market/), [Fortune BI](https://www.fortunebusinessinsights.com/digital-risk-protection-market-111208) |
| **A24** | 🇯🇵 APPI amendment | Bill submitted to the Diet **April 2026**. Jan 2026 PPC reform policy introduces **administrative fines for the first time**, expected by 2028. Risk-based individual notification | [Mori Hamada](https://www.morihamada.com/en/insights/newsletters/138006), [Baker McKenzie](https://www.bakermckenzie.com/en/insight/publications/2026/05/japan-appi-reform-key-changes) |
| **A25** | Geospatial imagery analytics | $16.19B (2026) @27.7% · also $36.01B **(2.2× spread)** | [TBRC](https://www.thebusinessresearchcompany.com/report/geospatial-imagery-analytics-global-market-report), [Fortune BI](https://www.fortunebusinessinsights.com/geospatial-imagery-analytics-market-108685) |
| **A26** | Cloud / data security posture | DSPM $2.20B (2025) → $6.19B (2033) · cloud security $34.37B (2026) → $59.34B (2031) @11.5% · CNAPP $16.8B (2026) → $38.0B (2030) @21.8% | [Grand View](https://www.grandviewresearch.com/industry-analysis/data-security-posture-management-market-report), [M&M](https://www.marketsandmarkets.com/PressReleases/cloud-security.asp) |
| **A27** | GRC / compliance automation | eGRC $57.10B (2026) @10.8% · GRC platforms $23.32B (2026) · **60–70% of first-time SOC 2 orgs use Vanta/Drata/Secureframe/Sprinto** · >70% of enterprise buyers require SOC 2 | [ComplyJet](https://www.complyjet.com/blog/soc-2-compliance-market-size), [Enzuzo](https://www.enzuzo.com/blog/best-grc-software) |
| **A28** | AI drug discovery | $5.00B (2026) · also $8.6B · also $24.51B **(7× spread)** · NA = 65.93% share (2025) | [Fortune BI](https://www.fortunebusinessinsights.com/artificial-intelligence-in-drug-discovery-market-105354), [Roots](https://www.rootsanalysis.com/reports/ai-based-drug-discovery-market.html) |
| **A29** | Femtech | $9.78B (2026) → $18.98B (2031) @14.2% · also $10.67B @18.37% · also $55.88B **(5.7× spread)** | [Mordor](https://www.globenewswire.com/news-release/2026/05/08/3290833/0/en/Femtech-Market-Outlook-2026-2031-Growing-at-14-2-CAGR-Driven-by-Digital-Women-s-Health-Adoption-Reports-Mordor-Intelligence.html), [Fortune BI](https://www.fortunebusinessinsights.com/femtech-market-107413) |
| **A30** | MDR | $5.09B (2026) → $13.45B (2031) @21.45% · also $3.92B → $13.90B (2035) · ~64% of enterprises use AI analytics in secops; false positives −39% | [Mordor](https://www.mordorintelligence.com/industry-reports/managed-detection-and-response-market), [Precedence](https://www.precedenceresearch.com/managed-detection-and-response-market) |
| **A31** | ITDR / non-human identity | $3.42B (2026) → $10.51B (2031) @25.17% · **API keys, service accounts and orphaned credentials = 41% of identity breaches** | [Mordor](https://www.mordorintelligence.com/industry-reports/identity-threat-detection-and-response-itdr-market) |
| **A32** | PTaaS | $0.72B (2026) → $1.98B (2031) @22.6% · also $1.20B · leaders Veracode, Synack | [M&M](https://www.marketsandmarkets.com/PressReleases/penetration-testing-as-a-service-ptaas-market-worth-1-98-billion-by-2031--marketsandmarkets-302739094.html) |
| **A33** | Revenue cycle management | RCM $95.22B (2026) · **AI-in-RCM $21.49B (2026) → $71.27B (2031) @27.1%** · >$500M invested in AI RCM in 2026 YTD · Candid Health $120M Series D on 2026-07-22 | [Mordor](https://www.mordorintelligence.com/industry-reports/ai-in-revenue-cycle-management-market), [Citrusbug](https://citrusbug.com/blog/revenue-cycle-management-statistics) |
| **A34** | Disinformation operations | $500M (2025) → $1.8B (2034) @15% | [SRI](https://www.openpr.com/news/4594962/disinformation-operations-market-to-reach-usd-1-8-billion) |
| **A35** | API security | $1.195B (2026), ~28.46% CAGR · vendors Salt Security, Kong, Microsoft, AWS; Akamai–Apiiro partnership | [Fortune BI](https://www.fortunebusinessinsights.com/api-security-market-111625) |
| **A36** | Pharmacovigilance | PV software $234.73M (2026) · PV & drug safety software $2.86B (2026) @12.8% · vendors IQVIA, Oracle, ArisGlobal, RXLogix | [Mordor](https://www.mordorintelligence.com/industry-reports/pharmacovigilance-and-drug-safety-software-market), [TBRC](https://www.thebusinessresearchcompany.com/report/pharmacovigilance-global-market-report) |
| **A37** | Health interoperability | Solutions $5.64B (2026) · data interop $8.61B (2026) → $14.98B (2030) @14.8% · **HL7 FHIR compliance $2.6B (2026) → $8.6B (2036) @12.7%** | [R&M](https://www.researchandmarkets.com/report/healthcare-interoperability-solution), [Morningstar/AccessWire](https://www.morningstar.com/news/accesswire/1146689msn/hl7-fhir-compliance-market-to-reach-usd-86-billion-by-2036-as-global-interoperability-mandates-accelerate-healthcare-api-standardization) |
| **A38** | Insider risk | $4.5B–$7.09B (2026) · **720% surge in exfiltration in the 24h before a layoff** · personal cloud 22.7%, removable media 15.6%, genAI tools 13.1% of incidents · avg annualized cost **$19.5M** (Ponemon/DTEX 2026) | [VMR](https://www.verifiedmarketreports.com/product/insider-risk-management-market/), [Vectra](https://www.vectra.ai/topics/insider-risk-management) |
| **A39** | 🇯🇵 Medical DX | Medical DX Promotion Plan (2022); nationwide EMR rollout; **My Number Card as health-insurance credential**; national medical information platform covering e-prescriptions, EMR, vaccination records | [Digital Agency](https://www.digital.go.jp/en/policies/health), [ITA](https://www.trade.gov/market-intelligence/japan-medical-digital-transformation) |
| **A40** | CGM / metabolic | $15.77B (2026) @15.09% · **OTC CGM $528.2M (2025) @17.1%** — the wellness wedge | [Mordor](https://www.mordorintelligence.com/industry-reports/continuous-glucose-monitoring-market), [GMI](https://www.gminsights.com/industry-analysis/otc-continuous-glucose-monitoring-market) |
| **A41** | Post-quantum crypto | $2.2B (2026) → $20.5B (2033) @37.8% · migration $2.41B (2026) · **61% plan to migrate within 5 years but only 41% are actively preparing** | [Grand View](https://www.grandviewresearch.com/industry-analysis/post-quantum-cryptography-market-report), [FMI](https://www.futuremarketinsights.com/reports/post-quantum-cryptography-pqc-migration-market) |
| **A42** | Healthcare fraud detection | $3.22B (2026) → $7.85B (2031) @19.54% · **$308.6B/yr lost to fraud, waste and abuse in US healthcare** (Coalition Against Insurance Fraud) · claims review = 49.9% of applications | [Mordor](https://www.mordorintelligence.com/industry-reports/healthcare-fraud-detection-market) |
| **A43** | 🇯🇵 SaMD regulation | **PMDA updated core SaMD guidance 2026-06-05** — AI/ML validation, cybersecurity expectations, classification rules. DASH for SaMD 2 (Sept 2023, 5-yr). **~100 AI SaMDs approved and reimbursed in Japan** | [PMDA](https://www.pmda.go.jp/english/review-services/reviews/0009.html), [GHM journal](https://www.jstage.jst.go.jp/article/ghm/8/3/8_2026.01052/_article/-char/en) |
| **A44** | BEC / cyber-enabled fraud | FBI IC3 2025: **24,768 BEC complaints, $3.05B losses**; total cybercrime losses **$20.9B (+26% YoY)**; $55.5B BEC over a decade; **avg loss $137,000/incident**; 28.6M phished identity records recaptured 2025 | [IC3 via SpyCloud](https://spycloud.com/blog/fbi-internet-crime-report-2025/), [HIPAA Journal](https://www.hipaajournal.com/fbi-bec-warning-55-billion-lost/) |
| **A45** | Healthcare ransomware | Avg breach **$7.42M** · **770 HIPAA breaches in 2025, a record** · H1 2026: **2.3 attacks/day**, 410 attacks (247 on providers), +14% vs H2 2025 · ransomware = 48% of confirmed breaches · Change Healthcare hit **192.7M individuals** · **in-hospital mortality +34–38% during an attack** | [Comparitech](https://www.comparitech.com/news/healthcare-ransomware-roundup-h1-2026-stats-on-attacks-ransoms-and-data-breaches/), [ORDR](https://ordr.net/blog/healthcare-cybersecurity-statistics-2026-report) |
| **A46** | Supply chain risk | SCRM $3.73B (2026) → $5.03B (2030) @7.8% · AI supply-chain risk intelligence +$1.21B (2026–30) @20.5% | [R&M](https://www.researchandmarkets.com/reports/6103638/supply-chain-risk-management-market-report), [Technavio](https://www.technavio.com/report/ai-for-supply-chain-risk-intelligence-platforms-market-industry-analysis) |
| **A47** | Genomics / precision medicine | Genomics $22.6B (2026) → $72.5B (2033) @18.2% · precision oncology $133.06B (2026) @11.47% · NGS $9.8B (2026) @18.9% · **precision medicine *software* only $81M (2026)** | [Grand View](https://www.grandviewresearch.com/industry-analysis/genomics-market), [Coherent](https://www.coherentmarketinsights.com/industry-reports/oncology-precision-medicine-market) |
| **A48** | Cyber workforce | ISC2 4.8M gap · **95% report ≥1 skill need; 88% suffered a security event due to a skills shortage** · 90% report gaps, worst in AI and cloud · 72% role fill rate · **budget replaced talent as the #1 barrier** | [ISC2](https://www.isc2.org/Insights/2025/12/2025-ISC2-Cybersecurity-Workforce-Study), [StationX](https://app.stationx.net/articles/cybersecurity-skills-gap-statistics) |
| **A49** | Vuln / exposure mgmt | Security & vulnerability mgmt $17.82B (2026) → $24.27B (2031) @6.4% · **CTEM $1.48B (2026) → $4.22B (2036) @11.0%** | [Mordor](https://www.mordorintelligence.com/industry-reports/security-and-vulnerability-management-market), [FMI](https://www.futuremarketinsights.com/reports/continuous-threat-exposure-management-market) |

---

## Part A — OSINT & investigations (001–034)

#### 001 · Kaiji — sanctions-evasion vessel intelligence
**Pitch.** Fuse AIS gaps, satellite radar and port-call records into ranked "this ship went dark on purpose" cases with an evidence pack a compliance officer can file.
**ICP.** Trade-finance banks, marine insurers, energy traders, sanctions desks.
**Market.** `A25` geospatial imagery analytics $16.19B (2026, TBRC) — but the addressable slice is maritime compliance, not all of geospatial. `A19` AML software $3.84B (2025) is the closer buyer budget. *SAM (my arithmetic, assumption stated):* if maritime compliance is 3–5% of AML tooling spend, that is ~$115–190M today.
**Wedge.** Everyone sells AIS-gap alerts; nobody sells the *filed report*. Sell the evidence pack, not the signal.
**Rivals.** Windward, Kpler, Lloyd's List Intelligence, Pole Star. Well funded, incumbent-entrenched.
**Pricing.** $60–200k/yr seat-bundled; per-report overage.
**Risk.** SAR imagery costs are a real COGS floor and the incumbents have volume contracts you won't match.
**Call.** `WEDGE` · Pull 4 · Moat 2 · TTR 3 — real demand, but you need a data deal before you need a product.

#### 002 · Hojin Graph — the Japanese corporate ownership graph
**Pitch.** Every Japanese legal entity (corporate number / 法人番号), its filings, officers, addresses and inferred ownership links, as an API and a graph UI in English and Japanese.
**ICP.** Foreign investors, KYB vendors, M&A diligence teams, journalists.
**Market.** `A19` AML/KYC → $5.36B by 2027 @17.3%; `A8` TPRM $10.60B (2026). *SAM:* Japan is ~6% of global GDP; if Japan-entity data is 2% of KYB tooling spend, ~$60–100M.
**Wedge.** Japanese entity data is public but genuinely hard: name normalization across kanji/kana/romaji, address formats, and the shikaisha/branch distinction defeat generic vendors. That difficulty *is* the moat.
**Rivals.** Teikoku Databank, Tokyo Shoko Research (deep but Japanese-only, non-API, expensive); Moody's/Dun & Bradstreet (shallow on Japan).
**Pricing.** $2k–20k/mo API tiers; per-lookup for long tail.
**Risk.** TDB/TSR have 100 years of relationships and non-public credit data you cannot legally replicate.
**Call.** `BUILD` · Pull 4 · Moat 4 · TTR 4 — the normalization work is the product, and it compounds.

#### 003 · Kuroji — Japanese-language adverse media screening
**Pitch.** Adverse-media and PEP screening that actually reads Japanese: local wires, regional papers, regulator press releases, 2ch/5ch-tier chatter, with fuzzy kanji name matching.
**ICP.** Banks and crypto exchanges operating in Japan; global KYB vendors who need a Japan module.
**Market.** `A19` AML software $3.84B (2025) → $10.74B (2035). *SAM:* adverse media is a minority of AML spend; Japan-specific coverage might be $80–150M — my derivation, not a sourced split.
**Wedge.** Global screening vendors transliterate Japanese names badly and miss regional sources entirely. Demonstrable false-negative rate against a competitor is your whole sales deck.
**Rivals.** Refinitiv World-Check, LexisNexis, ComplyAdvantage — all present in Japan, all weak on it.
**Pricing.** Per-screened-entity, $0.05–0.50, volume tiered.
**Risk.** You may end up an OEM component inside ComplyAdvantage rather than a company. That is a fine outcome; price for it.
**Call.** `BUILD` · Pull 4 · Moat 4 · TTR 4 — narrow, defensible, and acquirable by design.

#### 004 · Mokugeki — executive exposure & doxxing monitor
**Pitch.** Continuously map what the open internet knows about a named executive and their household — data-broker records, leaked credentials, property records, family social accounts — and drive removals.
**ICP.** Chief security officers at large enterprises; family offices.
**Market.** `A23` digital risk protection $1.20B (2025) → $5.51B (2034) @18.5%. *SAM:* executive protection is a slice; assume 8–12% → ~$100–145M.
**Wedge.** Sell to the physical-security budget, not the cyber budget. It is less contested and the buyer already spends on drivers and residence hardening.
**Rivals.** ZeroFox, BlackCloak, Constella, Picnic.
**Pricing.** $500–2,000 per protected principal per month.
**Risk.** Removal is a treadmill with no end state; churn when the CISO's sponsor leaves.
**Call.** `WEDGE` · Pull 3 · Moat 2 · TTR 5 — easy to start, hard to stay differentiated.

#### 005 · Deal Radar — M&A target pre-diligence in a day
**Pitch.** Point it at a target company; it returns litigation, sanctions, adverse media, leaked credentials, tech-stack, hiring signals and ownership graph as a structured red-flag memo.
**ICP.** Mid-market PE, corp dev teams, boutique advisors priced out of Big Four diligence.
**Market.** `A1` OSINT $21.06B (2026, Mordor) — but note the **4.3× spread** on this anchor; do not trust it as a level. *SAM:* deal-diligence tooling is small; if 1% of OSINT spend, ~$210M.
**Wedge.** Big Four diligence costs six figures and takes six weeks. A $5k, one-day screen that decides whether to *start* diligence is a different product with a different buyer.
**Rivals.** Nexis Diligence, Kroll, Sayari, generic LLM wrappers.
**Pricing.** $3–8k per target; annual seat packs.
**Risk.** Deal flow is cyclical; you sell into the first budget cut.
**Call.** `WEDGE` · Pull 3 · Moat 2 · TTR 4 — good business in a good year.

#### 006 · Tier-N — supply chain discovery from public record
**Pitch.** Infer your suppliers' suppliers from customs manifests, port records, corporate filings and job postings; alert when a tier-3 you never knew about goes bankrupt, gets sanctioned or burns down.
**ICP.** Automotive, electronics and pharma supply chain risk teams.
**Market.** `A46` SCRM $3.73B (2026) → $5.03B (2030) @7.8%; AI supply-chain risk intelligence +$1.21B (2026–30) @20.5%. `A8` TPRM $10.60B (2026).
**Wedge.** Everyone monitors tier-1 because tier-1 fills in a questionnaire. Tier-3 is where the shutdowns come from and nobody fills in anything — which is exactly why it must be inferred from public record.
**Rivals.** Interos, Everstream, Resilinc, Sayari, Exiger.
**Pricing.** $80–400k/yr by supplier count.
**Risk.** Customs data coverage collapses outside the US/India corridors; Japanese and EU trade data is far thinner. Be honest about coverage or you will be caught.
**Call.** `BUILD` · Pull 5 · Moat 4 · TTR 3 — the +54% rise in third-party breaches (`A8`) is the sales pitch, pre-written.

#### 007 · Chorus — coordinated inauthentic behaviour attribution
**Pitch.** Detect coordinated amplification networks by graph structure and timing rather than content, and produce an attribution report naming the cluster.
**ICP.** Platforms, election commissions, brand-safety teams, national CERTs.
**Market.** `A34` disinformation operations $500M (2025) → $1.8B (2034) @15%. Small.
**Wedge.** Content classifiers lose the arms race to generative models permanently. Network topology and posting cadence do not care what the text says — that is the durable signal.
**Rivals.** Graphika, Alethea, Logically, Social Links.
**Pricing.** $150–500k/yr; government-heavy.
**Risk.** Platform API access is the whole business and it is revocable at a stranger's discretion. This is a rented foundation.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 2 — technically excellent, commercially fragile.

#### 008 · Kansen — election & civic integrity monitoring
**Pitch.** Turnkey monitoring for a specific election: candidate impersonation, synthetic audio, coordinated narratives, with a hotline into platform trust-and-safety.
**ICP.** Election commissions, party compliance offices, national broadcasters.
**Market.** `A34` $500M (2025) and `A14` deepfake detection ~$15.7B by 2026 @~42% — the latter figure is aggressive and its underlying segmentation is unclear; discount it.
**Wedge.** Elections are date-certain events with panic budgets. Sell an 8-week engagement, not a subscription.
**Rivals.** Reality Defender, Logically, in-house platform teams.
**Pricing.** $200–800k per election cycle.
**Risk.** Lumpy, politically exposed revenue; one bad call and you are the story.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 3 — a services business wearing SaaS clothes.

#### 009 · Verity Claims — OSINT verification for insurance claims
**Pitch.** Cross-check claimed loss events against public record: weather, satellite, traffic cameras, business registries, social posts. Flag the 3% that need an adjuster visit.
**ICP.** P&C insurers, TPAs.
**Market.** `A42` healthcare fraud detection $3.22B (2026) is the closest sourced analogue; P&C fraud tooling had **no clean anchor** in this research — scored down accordingly.
**Wedge.** Insurers measure everything in loss-ratio points. A tool that pays for itself in one recovered fraudulent claim sells itself, if you can prove it on their historical book.
**Rivals.** Shift Technology, Verisk, FRISS.
**Pricing.** Per-claim $0.50–5, or gainshare on recovered leakage.
**Risk.** Gainshare pricing means a 12-month proof cycle before a dollar arrives.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 2 — strong logic, slow proof.

#### 010 · Trace — asset tracing for litigation and enforcement
**Pitch.** Given a judgment debtor, find the assets: property records, corporate interests, vessel and aircraft registrations, crypto wallets, across jurisdictions.
**ICP.** Litigation funders, enforcement lawyers, insolvency practitioners.
**Market.** `A1` OSINT (spread-flagged) and `A19` AML. *SAM:* asset tracing is a professional-services niche; software attach is perhaps $150–300M globally — my estimate, unsourced.
**Wedge.** Litigation funders will pay on contingency-adjacent terms because their whole model is already outcome-priced. Align with it.
**Rivals.** Sayari, Kroll, Nardello (services), Chainalysis (crypto only).
**Pricing.** $10–50k per matter; funder revenue share.
**Risk.** Jurisdictional patchwork means coverage claims are easy to overstate and easy to disprove in court.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 3 — high value per matter, low volume.

#### 011 · Deskbound — the investigative journalist's workbench
**Pitch.** Case-centric OSINT workspace: entity graph, document OCR and translation, timeline, source-provenance chain, collaborative annotation, export to publication.
**ICP.** Newsrooms, non-profit investigative outfits, academic researchers.
**Market.** No clean anchor. Newsroom software budgets are structurally tiny.
**Wedge.** None commercially. The good version of this is a public good, not a company.
**Rivals.** Aleph/OCCRP, DocumentCloud, Hunchly, Maltego.
**Pricing.** Whatever a grant covers.
**Risk.** Your best users are your least able to pay, permanently.
**Call.** `AVOID` · Pull 2 · Moat 2 · TTR 3 — build it as open source or don't build it as a business.

#### 012 · Chain of Custody — evidence integrity for human rights documentation
**Pitch.** Capture-to-court provenance for OSINT evidence: content hashing, capture attestation, tamper-evident logs, Berkeley Protocol-aligned export.
**ICP.** Human rights NGOs, war crimes investigators, ICC-adjacent bodies.
**Market.** No clean market anchor found. Donor-funded.
**Wedge.** Admissibility is a binary requirement — that is real product value. But the buyers are grant-funded and procurement takes years.
**Rivals.** eyeWitness to Atrocities, Hala Systems, Truepic (adjacent).
**Pricing.** Grant-funded deployments, $50–250k.
**Risk.** Mission-critical, budget-trivial. The worst quadrant.
**Call.** `AVOID` · Pull 2 · Moat 3 · TTR 2 — do it as a foundation, not a startup.

#### 013 · Counterfeit Net — marketplace and social counterfeit detection
**Pitch.** Crawl marketplaces and social commerce for counterfeit listings of a brand, cluster by seller identity, and file bulk takedowns.
**ICP.** Luxury, pharma, electronics and auto-parts brand protection teams.
**Market.** `A23` DRP $1.20B (2025) → $5.51B (2034) @18.5%; anti-phishing → $10.88B (2034). Note the **80× definitional spread** on the DRP anchor.
**Wedge.** Seller-identity clustering beats listing-level whack-a-mole: take down the seller, not the listing.
**Rivals.** MarqVision, Red Points, Corsearch, Group-IB.
**Pricing.** $40–250k/yr by SKU and marketplace count.
**Risk.** Crowded, price-competitive, and marketplaces keep changing their APIs and their willingness to cooperate.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 4 — real market, brutal margins.

#### 014 · Hiring Signal — competitive intelligence from job postings
**Pitch.** Infer a competitor's roadmap, tech stack, geographic expansion and burn from their job postings, employee moves and office leases.
**ICP.** Corp strategy, sales intelligence, hedge funds.
**Market.** No clean anchor; sits inside sales/market intelligence tooling.
**Wedge.** Thin. Job-posting scraping is a commodity and LinkedIn's terms make the good version legally fraught.
**Rivals.** Revelio Labs, LinkUp, Aura, Datapeople.
**Pricing.** $20–100k/yr.
**Risk.** hiQ-style litigation exposure is not hypothetical.
**Call.** `AVOID` · Pull 2 · Moat 1 · TTR 4 — commoditized and legally exposed at the same time.

#### 015 · Groundwork — pre-acquisition site risk from orbit
**Pitch.** For a physical site under acquisition or lease: environmental history, flood and liquefaction exposure, permit history, neighbouring industrial activity, from satellite and public record.
**ICP.** REITs, industrial real estate, insurers, corporate real estate teams.
**Market.** `A25` geospatial imagery analytics $16.19B (2026) @27.7% — 2.2× spread noted.
**Wedge.** In Japan specifically, liquefaction and flood hazard maps are public, granular, and almost never integrated into commercial site selection. That gap is a product.
**Rivals.** Descartes Labs, ICEYE (flood), JBA Risk, Zesty.ai.
**Pricing.** $500–3,000 per site report; enterprise portfolio subscriptions.
**Risk.** Transaction-linked demand is cyclical and rate-sensitive.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 4 — strongest as a Japan-first play.

#### 016 · Unrest — civil disruption early warning for facilities
**Pitch.** Watch permits, local social channels, and mobilization signals near a customer's named facilities; warn 24–72h ahead of disruption.
**ICP.** Multinational corporate security, logistics, retail chains.
**Market.** `A1` OSINT. *SAM:* corporate security intelligence subscriptions, my estimate $200–400M globally.
**Wedge.** Geofenced relevance. Global feeds drown the analyst; "within 2km of your Osaka DC" is the only alert they read.
**Rivals.** Dataminr, Crisis24, Samdesk, Factal.
**Pricing.** $50–300k/yr by site count.
**Risk.** Dataminr's platform-firehose access is a structural advantage you cannot buy.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 4 — good product, entrenched incumbent.

#### 017 · Tarmac — corporate aviation intelligence
**Pitch.** Track corporate aircraft movements to infer deal activity, facility visits and executive travel patterns.
**ICP.** Hedge funds, journalists, competitive intelligence.
**Market.** No clean anchor; alternative-data budgets.
**Wedge.** Weak. ADS-B is free, the datasets are commoditized, and blocking programs are expanding.
**Rivals.** FlightAware, ADS-B Exchange, Quandl-style alt-data resellers.
**Pricing.** $2–20k/mo alt-data feed.
**Risk.** Signal decays as blocking spreads; ethically and reputationally grubby.
**Call.** `AVOID` · Pull 2 · Moat 1 · TTR 5 — a feature, not a company.

#### 018 · Scout — technology scouting from patents and preprints
**Pitch.** Track a technology area across patent filings, preprints, grant awards and startup formation; surface who is actually shipping versus publishing.
**ICP.** Corporate R&D, VC, government innovation agencies.
**Market.** No clean anchor. Adjacent to `A28` AI drug discovery $5.00B (2026) for the pharma slice.
**Wedge.** Linking patents to *people* and their moves is the non-obvious part; the documents are already free.
**Rivals.** PatSnap, Clarivate, Dimensions, IPRally.
**Pricing.** $30–150k/yr.
**Risk.** PatSnap and Clarivate own the buyer relationship and bundle aggressively.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 4 — differentiate on the people graph or don't bother.

#### 019 · Unwind — beneficial ownership resolution
**Pitch.** Given a sanctioned or high-risk entity, resolve the ownership chain through shells and nominee structures to a natural person, with a confidence score per hop.
**ICP.** Banks, exporters, defence primes, export-control teams.
**Market.** `A19` AML software $3.84B (2025) → $10.74B (2035); `A8` TPRM $10.60B (2026).
**Wedge.** The 50-percent-rule calculation across a multi-hop chain is genuinely hard and genuinely mandatory. Sell the calculation, with its uncertainty exposed rather than hidden.
**Rivals.** Sayari, Kharon, Moody's Grid, Kpler.
**Pricing.** $75–400k/yr.
**Risk.** Kharon and Sayari are very good and very well funded here.
**Call.** `WEDGE` · Pull 5 · Moat 3 · TTR 3 — mandatory demand, strong incumbents.

#### 020 · Ledger Lens — crypto attribution for non-crypto companies
**Pitch.** Wallet screening and exposure scoring packaged for ordinary corporates who suddenly need it — ransomware payments, treasury exposure, vendor payouts.
**ICP.** Mid-market corporates, insurers, IR firms.
**Market.** `A19` AML. Chainalysis and TRM dominate the crypto-native buyer.
**Wedge.** The non-crypto buyer wants one answer once a year, not a platform. Price accordingly.
**Rivals.** Chainalysis, TRM Labs, Elliptic.
**Pricing.** $5–30k/yr low-volume tier.
**Risk.** The incumbents can offer this tier at zero marginal cost the day they notice you.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 5 — you are building an incumbent's future free tier.

#### 021 · Ghost Candidate — recruitment fraud detection
**Pitch.** Detect fabricated identities, deepfaked video interviews and proxy candidates in remote hiring pipelines.
**ICP.** Enterprise talent acquisition, staffing firms, defence contractors.
**Market.** `A14` deepfake detection and voice cloning $4.06B (2026) @23.9%; **1 in 5 biometric fraud attempts now involve deepfakes**; **vishing +1,633% in Q1 2025**.
**Wedge.** North Korean IT-worker infiltration turned this from an HR annoyance into a board-level security issue with named enforcement actions behind it. That reframing is the sale.
**Rivals.** Persona, iDenfy, Reality Defender, HireRight (adjacent).
**Pricing.** Per-interview $1–10; enterprise floors.
**Risk.** False accusations against real candidates are a discrimination-litigation vector. Design for human review or don't ship.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — the threat is real, named, and currently unaddressed at the interview layer.

#### 022 · Pre-Hire Risk — open-source background screening
**Pitch.** Automated open-source screening of candidates beyond criminal record checks.
**ICP.** Enterprise HR.
**Market.** `A38` insider risk $4.5–7.09B (2026).
**Wedge.** None worth having.
**Rivals.** Fama, Ferretly, HireRight.
**Pricing.** Per-check $10–50.
**Risk.** FCRA in the US, GDPR Article 22 in the EU, and APPI purpose-limitation in Japan all point the same direction: automated adverse inference about individuals from social data is a legal minefield, and the discrimination exposure is asymmetric — you capture the downside, the customer captures the upside.
**Call.** `AVOID` · Pull 2 · Moat 1 · TTR 4 — legally and ethically the worst idea in this document. Included so the list is honest about what was considered and rejected.

#### 023 · Lookalike — domain and brand impersonation defence
**Pitch.** Detect typosquats and impersonation domains at registration time, score them, and automate takedown through registrar and hosting abuse channels.
**ICP.** Any consumer-facing brand; banks first.
**Market.** `A23` anti-phishing → $10.88B (2034) @13.8%; `A44` **$3.05B in BEC losses in 2025, avg $137k/incident**.
**Wedge.** Detection is commoditized; *takedown median time* is the only metric buyers actually compare. Compete on operational relationships with registrars, which is a services moat wearing a SaaS badge.
**Rivals.** ZeroFox, Bolster, Netcraft, Group-IB, Doppel.
**Pricing.** $25–150k/yr.
**Risk.** Crowded and increasingly bundled free into email security suites.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 5 — easy to build, hard to win.

#### 024 · Channels — closed-messaging threat monitoring
**Pitch.** Coverage of Telegram, Discord and forum ecosystems where initial-access brokers and ransomware crews actually operate, with entity extraction into a customer's TIP.
**ICP.** Threat intel teams at large enterprises; MSSPs.
**Market.** `A2` threat intelligence $11.55B (2025) → $22.97B (2030) @14.7%; TIP subsegment @20.15%. `A10` dark web monitoring $1.2B (2025).
**Wedge.** Access and persona maintenance is an operational discipline, not an engineering one. That is why it stays defensible — and why it is expensive to run.
**Rivals.** Flashpoint, Recorded Future, Intel 471, KELA, Cybersixgill.
**Pricing.** $60–300k/yr.
**Risk.** Persona burn, platform crackdowns, and genuine legal exposure in some jurisdictions.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 2 — the moat is real but it is a human moat, so it does not scale like software.

#### 025 · Rouei — credential leak monitoring for Japanese enterprises
**Pitch.** Monitor combolists, stealer logs and paste sites for credentials tied to Japanese corporate domains, with Japanese-language triage and an incident workflow that matches how a Japanese IT department actually escalates.
**ICP.** Japanese enterprises and their MSSPs.
**Market.** `A4` Japan cybersecurity $11.36B (2026) → $18.76B (2031) @10.57%; `A10` **6.8B stolen credentials traded in 2025 (+22% YoY), 73% appearing within 48 hours**.
**Wedge.** Global credential-monitoring vendors sell an English console into a market where the security operator often is not the English speaker. Localization here is not translation — it is the escalation model, the report format, and the vendor relationship.
**Rivals.** SpyCloud, Have I Been Pwned (free tier), Recorded Future, domestic SIers.
**Pricing.** ¥300k–3M/mo depending on domain and subsidiary count.
**Risk.** Japanese enterprise procurement strongly favours incumbent SIers; you will likely sell *through* NTT Data or NRI rather than around them. Plan for that margin.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — clear regulatory tailwind (`A11`, `A24`) meeting a genuinely underserved language market.

#### 026 · Range — OSINT training and analyst simulation
**Pitch.** Scenario-based OSINT training with scored investigations against a synthetic-but-realistic corpus, mapped to analyst competency frameworks.
**ICP.** Government agencies, banks' investigation teams, universities.
**Market.** `A48` cyber workforce: **95% of orgs report at least one skill need; 88% suffered a security event attributable to a skills shortage; 90% report gaps**. But note `A48` also says **budget replaced talent as the #1 barrier** — the pain is real and the money is not.
**Wedge.** Certification bodies, not enterprises, are the buyer with an actual budget line.
**Rivals.** SANS, Maltego Academy, Hack The Box (adjacent).
**Pricing.** $500–2,000 per seat per year.
**Risk.** Training budgets are the first cut, every cycle, and `A48` says the cut already happened.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 4 — the ISC2 data argues against this one more than for it.

#### 027 · Erase — personal data broker removal
**Pitch.** Automated opt-out and removal across data brokers and people-search sites, with continuous re-check.
**ICP.** Consumers; enterprise perk channel.
**Market.** `A23` DRP. Consumer privacy subscriptions.
**Wedge.** None. The category is a price war and the incumbents bundle it with antivirus for free.
**Rivals.** DeleteMe, Incogni, Optery, Norton/McAfee bundles.
**Pricing.** $10–20/mo consumer.
**Risk.** CAC exceeds LTV in a category where Norton gives it away.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 5 — structurally unprofitable at consumer CAC.

#### 028 · Waypoint — corporate travel risk intelligence
**Pitch.** Pre-trip risk briefings and in-trip alerting driven by the traveller's actual itinerary, integrated with the corporate travel booking tool.
**ICP.** Global mobility and duty-of-care teams.
**Market.** No clean anchor; duty-of-care spend sits inside travel management.
**Wedge.** Itinerary integration. A generic country-risk PDF is worthless; "your 14:20 flight lands in a city with an active transport strike" is not.
**Rivals.** International SOS, Crisis24, Riskline, Safeture.
**Pricing.** $15–60 per traveller per year.
**Risk.** International SOS bundles medical evacuation, which you cannot match and which is why the buyer signs.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 4 — the incumbent's bundle is the barrier, not the technology.

#### 029 · Greenwash — ESG claim verification from public record
**Pitch.** Test a company's public sustainability claims against regulatory filings, satellite-observed activity, enforcement records and local reporting.
**ICP.** Asset managers, ESG rating agencies, procurement teams under CSRD-style obligations.
**Market.** `A25` geospatial; `A46` supply chain risk $3.73B (2026).
**Wedge.** Contradiction detection between a company's own statements and observable reality is a cleaner product than yet another ESG score — scores are saturated, contradictions are evidence.
**Rivals.** RepRisk, Clarity AI, Sustainalytics, Satelligence.
**Pricing.** $50–250k/yr.
**Risk.** ESG budgets have been contracting and the political environment in the US is actively hostile.
**Call.** `WATCH` · Pull 2 · Moat 3 · TTR 3 — right product, wrong half-decade.

#### 030 · Dark Fleet — IUU fishing and fisheries compliance
**Pitch.** Detect illegal, unreported and unregulated fishing from AIS gaps, VIIRS night lights and SAR, for enforcement agencies and seafood buyers.
**ICP.** Coastal state enforcement, seafood importers with due-diligence obligations.
**Market.** `A25` geospatial imagery analytics; `A46` supply chain risk.
**Wedge.** Import-control regimes turn this from an NGO concern into an importer compliance requirement with a customs consequence.
**Rivals.** Global Fishing Watch (free and excellent), Windward, OceanMind.
**Pricing.** $40–200k/yr importer subscriptions.
**Risk.** Global Fishing Watch gives away the core dataset. You must sell the compliance artefact, not the detection.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 3 — viable only on the importer side of the market.

#### 031 · Damage — conflict and disaster damage assessment
**Pitch.** Rapid building-level damage assessment from pre/post imagery, delivered as structured layers for insurers, reconstruction agencies and humanitarian responders.
**ICP.** Reinsurers, government disaster agencies, UN bodies.
**Market.** `A25` geospatial imagery analytics $16.19B (2026) @27.7%.
**Wedge.** In Japan, earthquake and typhoon response is a recurring, funded, date-uncertain-but-certain event. A pre-positioned contract with a prefecture is a real business.
**Rivals.** ICEYE, Descartes Labs, Planet, Maxar.
**Pricing.** Event-triggered $50–500k; standby retainers.
**Risk.** Revenue is literally disaster-dependent, which makes forecasting and staffing hard.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — retainer model or nothing.

#### 032 · Chiban — foreign land ownership transparency (Japan)
**Pitch.** Track ownership of land near sensitive sites — bases, water sources, infrastructure — through registry records and corporate ownership chains.
**ICP.** Japanese national and prefectural government, defence-adjacent policy bodies.
**Market.** No sourced market figure. Policy-driven procurement under Japan's land-use regulation regime for important facility areas.
**Wedge.** The registry data is public but fragmented across legal affairs bureaus and effectively unqueryable at scale. Making it queryable is the entire product.
**Rivals.** Domestic SIers; no clear specialist.
**Pricing.** Government contract, ¥50–300M.
**Risk.** Single-buyer concentration and acute political sensitivity around how results get used.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 2 — genuine moat, but you are building for one customer.

#### 033 · Gatekeep — research security screening for universities
**Pitch.** Screen research collaborations, visiting scholars and funding sources against export-control lists, foreign talent programmes and undisclosed affiliations.
**ICP.** University research offices, national labs, corporate R&D.
**Market.** `A19` AML/KYC screening infrastructure reused for a different buyer; `A8` TPRM $10.60B (2026).
**Wedge.** Universities are being handed a compliance obligation they have no compliance function to meet. That gap is the opportunity — and it repeats in every jurisdiction that adopts research-security rules.
**Rivals.** Clarivate, Interfolio (adjacent), consultancies.
**Pricing.** $30–150k/yr per institution.
**Risk.** University procurement is slow, poor and committee-driven; and the work has a real chance of being applied in discriminatory ways, which you must design against explicitly.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — growing mandate, difficult buyer, real ethical care required.

#### 034 · Provenance — evidence lineage as infrastructure
**Pitch.** A layer under any OSINT tool that records where every displayed record came from, when, from which fetch, and whether anything was dropped between collection and display.
**ICP.** OSINT platform vendors, government analytic shops, regulated investigators.
**Market.** No clean anchor — it is infrastructure, sold into `A1`/`A2` budgets.
**Wedge.** This is the productized form of the two rules this repository runs on: never fabricate, never silently discard. Every analytic platform claims completeness; none can prove it. A tool that makes truncation and fetch-failure *visible as data* rather than as a log line is the difference between an analyst trusting the dashboard and quietly rebuilding it in a spreadsheet.
**Rivals.** None directly; partially covered by Palantir's internal lineage and by open provenance standards (W3C PROV).
**Pricing.** OEM licensing $50–300k/yr; per-seat in regulated deployments.
**Risk.** Infrastructure that makes a vendor's coverage gaps visible is a hard thing to sell *to that vendor*. Sell to the buyer who audits them.
**Call.** `BUILD` · Pull 3 · Moat 4 · TTR 3 — small market, but the one concept here that this codebase has already proven can be built.


## Part B — Cybersecurity (035–092)

> Six of the first eight entries in this section rest on `A11` — Japan's Active
> Cyber Defense Law, whose main provisions take effect **2026-10-01**, seven
> weeks after this document was written. A dated statutory obligation is the
> single most reliable source of enterprise software demand that exists. It is
> also a wasting asset: by 2028 these will be commodity compliance features
> inside the incumbent SIer bundles. The window is now, and it is short.

#### 035 · Todoke — ACD Law incident reporting automation
**Pitch.** Turn a security incident into a compliant regulator notification under the Active Cyber Defense Law: scope determination, deadline tracking, sector-specific form generation, evidence attachment, submission log.
**ICP.** The 15 critical-infrastructure sectors named in `A11` — electricity, gas, telecoms, finance, transport, healthcare and the rest.
**Market.** `A11` — mandatory incident reporting begins **2026-10-01**. `A4` Japan cybersecurity $11.36B (2026) → $18.76B (2031) @10.57%. *SAM (my arithmetic):* if there are on the order of a few thousand designated operators and each pays ¥3–10M/yr, that is roughly ¥10–30B (~$70–200M). The operator count is my assumption — `A11` names sectors, not headcounts.
**Wedge.** Deadline arithmetic under stress. The failure mode is not "we didn't know we had to report", it is "we missed the window while arguing about whether it qualified". Sell the clock.
**Rivals.** NTT Data, NRI, Hitachi and the other SIers will build this. They will build it slowly and as part of a ¥100M engagement.
**Pricing.** ¥3–10M/yr, seat-independent.
**Risk.** Enforcement detail is still settling and full effect is not until 2027; you may build to a spec that moves.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — the clearest dated demand signal in this entire document.

#### 036 · Kijun — Critical System notification register
**Pitch.** Maintain the register of systems that meet the `A11` "Critical System" threshold, track when a change triggers a notification duty, and produce the filing.
**ICP.** Same 15 sectors; specifically the architecture and governance functions.
**Market.** `A11`; `A4`.
**Wedge.** The duty attaches to *introducing* certain systems — meaning it fires during projects, not during incidents. It belongs in the change-management flow, not the SOC. Nobody is building it there.
**Rivals.** ServiceNow if it notices; domestic SIers.
**Pricing.** ¥5–15M/yr.
**Risk.** Narrow enough that it wants to be a module of #035 rather than a company. Consider building them together.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — merge with #035 unless there is a reason not to.

#### 037 · Renkei — supply chain security requirement cascade
**Pitch.** Large operators must push security requirements down to suppliers who have no security function. This distributes the requirement, collects evidence, and scores the supplier — in Japanese, at SME reading level.
**ICP.** Prime contractors in the `A11` sectors; their thousands of tier-2/3 suppliers.
**Market.** `A11` explicitly notes that SMEs face no uniform reporting duty **but will be asked to meet security requirements as suppliers to critical infrastructure**. `A8` TPRM $10.60B (2026) → $20.71B (2031) @14.34%; third-party breaches **+54% (2020–2024)**.
**Wedge.** The prime pays, the supplier uses. That solves the SME's zero-budget problem, which is the reason every SME security product fails in Japan.
**Rivals.** Panorays, SecurityScorecard, Assured (Japan), domestic questionnaire consultancies.
**Pricing.** Prime pays ¥10–50M/yr for N suppliers; supplier seat free.
**Risk.** You are selling into a relationship, not a company — if the prime changes procurement leadership the whole cascade stalls.
**Call.** `BUILD` · Pull 5 · Moat 4 · TTR 3 — the payer/user split is the insight; it is what makes this work where SME security does not.

#### 038 · Keiretsu Map — subsidiary attack surface discovery
**Pitch.** Discover the internet-facing estate of a Japanese conglomerate *including* the subsidiaries, joint ventures and affiliates that the parent's IT department has never inventoried.
**ICP.** Group CISOs at Japanese holding companies.
**Market.** `A5` ASM $1.65–2.03B (2026), up to 31.3% CAGR → $5B (2034). `A4` Japan cybersecurity.
**Wedge.** Japanese group structures are unusually deep and unusually decentralized in IT. Generic ASM finds the parent's domains; it does not find the 1998-vintage web server at a 40%-owned subsidiary in Kyushu. The corporate-structure resolution from #002 is the differentiator here.
**Rivals.** All of `A5`'s vendor list — Microsoft, Tenable, CrowdStrike, CyCognito, Bitsight, and the rest — plus domestic MSSPs.
**Pricing.** ¥15–80M/yr by group entity count.
**Risk.** ASM is consolidating into platforms fast; standalone ASM is being bundled away.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — only defensible if paired with real entity-resolution data.

#### 039 · Reachable — exploitability-first exposure management
**Pitch.** Rank vulnerabilities by whether an attacker can actually reach them in *this* environment — network path, authentication requirement, exploit maturity — not by CVSS.
**ICP.** Enterprise vulnerability management teams drowning in findings.
**Market.** `A49` security & vulnerability management $17.82B (2026) → $24.27B (2031) @6.4%; CTEM $1.48B (2026) → $4.22B (2036) @11.0%. Note the CTEM growth rate (11%) is *lower* than most of this document's markets — the category is more talked about than bought.
**Wedge.** Everyone claims prioritization. Almost nobody proves reachability with an actual path. Proof is the product.
**Rivals.** Tenable, Rapid7, Qualys, Vulcan (acquired), XM Cyber, Pentera.
**Pricing.** $50–400k/yr by asset count.
**Risk.** The `A49` 6.4% CAGR on the core market says this is a mature, share-shift fight, not a growth market. You are taking budget from Tenable, not finding new budget.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 3 — the anchor's own growth rate argues against it.

#### 040 · Kikai — non-human identity governance
**Pitch.** Inventory every service account, API key, workload identity and agent credential; assign an owner; enforce rotation; kill the orphans.
**ICP.** Cloud-heavy enterprises, financial services.
**Market.** `A31` ITDR $3.42B (2026) → $10.51B (2031) @25.17%, and the datapoint that matters: **API keys, service accounts and orphaned credentials account for 41% of identity breaches.**
**Wedge.** Human IAM is a solved, consolidated market. Non-human identity outnumbers human identity by an order of magnitude in a modern estate and has almost no lifecycle governance. The 41% figure is the entire pitch.
**Rivals.** Astrix, Entro, Oasis, Silverfort, CyberArk (moving in), Okta (moving in).
**Pricing.** $60–350k/yr by identity count.
**Risk.** CyberArk and Okta both want this and both can bundle it. Assume an acquisition outcome, not an IPO one.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — best single statistic-to-product fit in the security half of this list.

#### 041 · Leakproof — secrets sprawl detection across the whole estate
**Pitch.** Find hardcoded credentials in repos, CI logs, container layers, wikis, ticket attachments and chat history — then verify which are still live.
**ICP.** Platform engineering and AppSec teams.
**Market.** `A31` ITDR; `A18` software supply chain security $2.16B (2026).
**Wedge.** Detection is table stakes; **liveness verification** is not. "1,400 secrets found" is noise. "9 are still valid and one is a production database" is an incident.
**Rivals.** GitGuardian, Nightfall, TruffleHog (open source), GitHub secret scanning (free and bundled).
**Pricing.** $20–150k/yr.
**Risk.** GitHub gives away the repo case, which is the case most buyers think they have.
**Call.** `WEDGE` · Pull 4 · Moat 2 · TTR 5 — only viable if you own the non-repo surfaces.

#### 042 · Rotate — credential rotation orchestration
**Pitch.** Actually rotate the credentials that #040 and #041 find, with dependency mapping so rotation doesn't take production down.
**ICP.** Same as #040.
**Market.** `A31` ITDR.
**Wedge.** Discovery tools stop at the list because rotation is the part that can break things. The dependency graph that makes rotation safe is the hard, valuable half.
**Rivals.** HashiCorp Vault, CyberArk, AWS Secrets Manager (each partial).
**Pricing.** $50–250k/yr.
**Risk.** You are one bad rotation away from causing a customer outage, and that story travels.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 3 — high moat because it is genuinely dangerous to build.

#### 043 · Grant — SaaS-to-SaaS OAuth risk
**Pitch.** Map every third-party app an employee has granted access to corporate SaaS, score the permission scope against the vendor's actual security posture, and revoke at scale.
**ICP.** IT and security in SaaS-first companies.
**Market.** `A8` TPRM $10.60B (2026); `A31` ITDR.
**Wedge.** The risk is a *permission grant made by an employee in 15 seconds* that no procurement process ever saw. That is a different discovery problem from vendor management.
**Rivals.** Obsidian, AppOmni, Valence, Nudge Security, Microsoft Defender for Cloud Apps.
**Pricing.** $30–200k/yr.
**Risk.** Microsoft bundles a decent version into E5, which most of your buyers already own.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 5 — good problem, bad competitive geometry.

#### 044 · Shadow — unsanctioned AI tool discovery
**Pitch.** Find which employees are pasting what into which AI tools, from network telemetry and browser signals; classify the data sensitivity; give the security team a factual basis for policy.
**ICP.** Enterprises with an AI policy nobody can enforce.
**Market.** `A12` AI governance $227–340M (2024–25) → $4.83B (2034) @35–45%; **only 6% of organizations have advanced AI security strategies**. `A38` insider risk: **genAI tools already account for 13.1% of insider exfiltration incidents.**
**Wedge.** That 13.1% figure converts an abstract governance worry into a measured exfiltration channel with a number attached. Lead with it.
**Rivals.** Netskope, Zscaler, Harmonic, Prompt Security, Witness.
**Pricing.** $25–150k/yr.
**Risk.** This is a feature of the SASE platforms and they know it.
**Call.** `WATCH` · Pull 5 · Moat 1 · TTR 5 — enormous pull, essentially no moat.

#### 045 · Ledger — AI agent action audit trail
**Pitch.** An immutable, queryable record of what every autonomous agent did: which tool, which arguments, on whose authority, with what result — reconstructable months later for an auditor.
**ICP.** Regulated enterprises deploying agents; anyone whose auditor has started asking.
**Market.** `A12` AI governance; **40% of enterprise applications are expected to embed autonomous agents by end-2026, and only 6% of organizations have advanced AI security strategies** — that gap is the market.
**Wedge.** Observability tools answer "did it work". Audit answers "who is accountable and can you prove it". Different artefact, different buyer, and the second one is legally compelled while the first is optional.
**Rivals.** Langfuse, Braintrust, Arize, Credo AI, IBM watsonx.governance.
**Pricing.** $40–300k/yr.
**Risk.** The observability vendors are one schema change away from claiming this, and "audit-grade" is easy to assert and hard for a buyer to evaluate.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — the agent-adoption curve and the governance-readiness curve have diverged, and the gap is where the money is.

#### 046 · Injection — continuous prompt injection testing
**Pitch.** Continuously attack a customer's own LLM applications with an evolving corpus of injection and jailbreak techniques; report what got through, with reproduction steps.
**ICP.** Companies shipping LLM features to customers.
**Market.** `A12` AI governance; `A32` PTaaS $0.72B (2026) → $1.98B (2031) @22.6%.
**Wedge.** Regression testing, not a one-off report. Every model update silently changes the safety surface, which makes this recurring by nature — the rare security product whose renewal logic is structural.
**Rivals.** Lakera, Robust Intelligence (Cisco), HiddenLayer, Promptfoo (open source), Garak.
**Pricing.** $30–200k/yr.
**Risk.** Crowded with well-funded entrants and open-source alternatives that are genuinely good.
**Call.** `WEDGE` · Pull 4 · Moat 2 · TTR 5 — win on the regression-over-time framing or lose.

#### 047 · Red Cell — AI red teaming as a managed service
**Pitch.** Human-led adversarial testing of AI systems, delivered as a recurring engagement with a platform underneath.
**ICP.** Foundation model labs, large deployers, regulated industries.
**Market.** `A12`; `A32` PTaaS.
**Wedge.** Human creativity still beats automated corpora on novel jailbreaks, and regulators increasingly want a named human to have signed something.
**Rivals.** Trail of Bits, NCC Group, Lakera, the labs' internal teams.
**Pricing.** $100–500k per engagement.
**Risk.** It is a consultancy. The gross margin will tell you so every month.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 4 — good revenue, bad multiple.

#### 048 · MBOM — model bill of materials
**Pitch.** Track which models, weights, datasets, fine-tunes and licences are in production, where they came from, and what obligations attach.
**ICP.** Enterprises with model sprawl; anyone facing EU AI Act documentation duties.
**Market.** `A12` AI governance; `A18` SBOM management & compliance → $9.7B (2035) @13.2%.
**Wedge.** SBOM took a decade and an executive order to become mandatory. Model provenance is on the same trajectory with a shorter fuse. Being early to a mandate is a real strategy — see #049.
**Rivals.** Protect AI (acquired by Palo Alto), JFrog ML, Hugging Face (partial, free).
**Pricing.** $40–200k/yr.
**Risk.** Early. The mandate is coming, but "coming" has killed a lot of companies.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 4 — right thesis, timing risk is the whole question.

#### 049 · CBOM — cryptographic asset inventory for PQC
**Pitch.** Discover every use of cryptography across an estate — libraries, certificates, protocols, hardware, embedded — and produce a migration-ready cryptographic bill of materials.
**ICP.** Banks, telecoms, government, anyone with a "harvest now, decrypt later" exposure.
**Market.** `A41` PQC $2.2B (2026) → $20.5B (2033) **@37.8% — the highest CAGR of any anchor in this document**. And the gap that matters: **61% of organizations plan to migrate within five years, but only 41% are actively preparing**, with "poor visibility into cryptographic assets" named as a top blocker.
**Wedge.** You cannot migrate what you cannot find, and discovery is unambiguously step one of every published migration methodology. This is the pick-and-shovel position in the fastest-growing market found in this research.
**Rivals.** SandboxAQ, InfoSec Global, Keyfactor, Venafi (CyberArk), IBM Quantum Safe.
**Pricing.** $100–600k/yr enterprise.
**Risk.** Long sales cycles, and the threat's timeline is genuinely uncertain — a buyer can always defer one more year.
**Call.** `BUILD` · Pull 5 · Moat 4 · TTR 3 — highest-growth anchor, a named blocker, and a defensible technical position. Strongest cyber concept in this document.

#### 050 · Migrate — PQC migration orchestration
**Pitch.** Sequence and execute the crypto migration: dependency ordering, hybrid rollout, compatibility testing, rollback.
**ICP.** Same as #049, one step later.
**Market.** `A41` PQC migration $2.41B (2026).
**Wedge.** The natural expansion from #049 — and the reason #049 is worth more than a scanner.
**Rivals.** IBM (with Bain, per `A41`), Thales, Entrust, the big SIers.
**Pricing.** $200k–2M multi-year.
**Risk.** Standalone, this is professional services. It only works as the second act of an inventory product.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 2 — build #049 first, then this.

#### 051 · Expiry — certificate lifecycle under shortening validity
**Pitch.** Automated certificate discovery, issuance and renewal built for a world where TLS certificate lifetimes keep shrinking and manual renewal is arithmetically impossible.
**ICP.** Every enterprise with more than a few hundred certificates.
**Market.** `A41` PQC (crypto agility overlaps directly).
**Wedge.** The shortening-validity trend converts a chore into an automation requirement with a hard deadline. But Venafi and Keyfactor built exactly this and own the category.
**Rivals.** Venafi (CyberArk), Keyfactor, DigiCert, Let's Encrypt + cert-manager (free, and sufficient for most).
**Pricing.** $50–400k/yr.
**Risk.** Mature category, free open-source alternative that covers the common case well.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 4 — real need, thoroughly served.

#### 052 · Consume — SBOM ingestion, VEX and the "so what" layer
**Pitch.** Take in the SBOMs your suppliers now send you, correlate against vulnerability and VEX data, and answer the only question that matters: which of these actually affect us.
**ICP.** Enterprises and agencies receiving supplier SBOMs; medical device and automotive OEMs.
**Market.** `A18` SBOM management & compliance → $9.7B (2035) @13.2%; note the narrower software-supply-chain-security anchor grows at only **4.72%**, a 3× disagreement about the same space.
**Wedge.** The mandates created SBOM *production*. Almost nothing was built for *consumption*, and the receiving organization is the one with the obligation and the liability.
**Rivals.** Anchore, Cybeats, Manifest, Finite State (device-focused).
**Pricing.** $40–250k/yr.
**Risk.** SBOM quality in the wild is poor enough that your output is limited by your input, and customers will blame you for it.
**Call.** `BUILD` · Pull 4 · Moat 3 · TTR 4 — the consumption side is genuinely underbuilt.

#### 053 · Bus Factor — open source maintainer risk
**Pitch.** Score the dependencies you rely on by maintainer concentration, funding, responsiveness and takeover risk — the xz-utils failure mode, before it happens.
**ICP.** Platform engineering, OSPOs, security architecture.
**Market.** `A18` software supply chain security $2.16B (2026).
**Wedge.** Every other tool scores *known vulnerabilities* in dependencies. This scores the *probability of future compromise* from social factors. Genuinely novel; also genuinely hard to prove value for until the day it is proven catastrophically.
**Rivals.** Socket, Chainguard (different approach), OpenSSF Scorecard (free).
**Pricing.** $30–150k/yr.
**Risk.** Nobody has a budget line for "a maintainer might get socially engineered in 18 months".
**Call.** `WATCH` · Pull 2 · Moat 4 · TTR 4 — brilliant idea, no purchase order.

#### 054 · Attest — build provenance and SLSA compliance
**Pitch.** Generate and verify signed build provenance end-to-end, so a deployed artefact can be traced to the exact source and builder.
**ICP.** Software vendors selling into government and regulated buyers.
**Market.** `A18`.
**Wedge.** Thin — Sigstore is free, excellent and increasingly built into the CI platforms.
**Rivals.** Sigstore (free), Chainguard, GitHub Actions attestations (bundled).
**Pricing.** $20–100k/yr.
**Risk.** Competing against free infrastructure backed by the Linux Foundation and GitHub.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 4 — the free version is the standard.

#### 055 · Drift — runtime-to-build container reconciliation
**Pitch.** Detect when a running container no longer matches what the pipeline built — packages installed at runtime, files changed, processes that shouldn't exist.
**ICP.** Container-heavy platform teams.
**Market.** `A26` CNAPP $16.8B (2026) → $38.0B (2030) @21.8%.
**Wedge.** Modest; the CNAPP platforms treat this as a checkbox.
**Rivals.** Wiz, Sysdig, Aqua, Palo Alto Prisma, Falco (free).
**Pricing.** $40–200k/yr.
**Risk.** CNAPP consolidation has already happened. Point tools in this space get acquired cheaply or die.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 4 — consolidation closed this window.

#### 056 · Autofix — infrastructure-as-code remediation autopilot
**Pitch.** Don't report the misconfiguration; open the pull request that fixes it, with the blast-radius analysis attached.
**ICP.** Platform teams with large Terraform estates.
**Market.** `A26` cloud security $34.37B (2026) → $59.34B (2031) @11.5%.
**Wedge.** The gap between "finding" and "fixed" is where security tools go to die. Closing it with a reviewable PR — not an auto-apply — is the version engineers accept.
**Rivals.** Wiz, Snyk IaC, Firefly, Resourcely.
**Pricing.** $40–250k/yr.
**Risk.** Trust. One bad auto-generated PR merged at 2am and you are uninstalled.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — the PR-not-autoapply distinction is the whole product decision.

#### 057 · Rightsize — cloud entitlement reduction
**Pitch.** Compare granted cloud permissions against permissions actually used over 90 days, and generate the reduced policy.
**ICP.** Cloud security and FinOps teams.
**Market.** `A26` cloud security; `A31` ITDR.
**Wedge.** Little. CIEM is a standard CNAPP module and the cloud providers ship analyzers themselves.
**Rivals.** Wiz, Palo Alto, Sonrai, AWS IAM Access Analyzer (free).
**Pricing.** $30–150k/yr.
**Risk.** Absorbed into platforms already.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 5 — a settled category.

#### 058 · Shares — DSPM for the file server nobody has opened since 2015
**Pitch.** Classify and remediate sensitive data in unstructured on-premises storage — the SMB shares, the SharePoint sprawl, the departmental NAS.
**ICP.** Large enterprises with decades of accreted storage; heavily Japanese-relevant.
**Market.** `A26` DSPM $2.20B (2025) → $6.19B (2033); the DSPM tool subsegment is quoted at **24.8% CAGR**, versus 13.9% for the broader figure — flagging the disagreement.
**Wedge.** DSPM vendors chase cloud data because it has APIs. The regulated, embarrassing, breach-causing data is on a file server in a basement, and `A24`'s incoming APPI administrative fines will make Japanese enterprises care about exactly that.
**Rivals.** Varonis (strong here), Cyera, Microsoft Purview, BigID.
**Pricing.** $60–400k/yr.
**Risk.** Varonis has owned unstructured data for 15 years and is very good at it.
**Call.** `WEDGE` · Pull 4 · Moat 2 · TTR 3 — Japan-first framing is the only angle that beats Varonis.

#### 059 · Kokunai — data residency enforcement for APPI cross-border rules
**Pitch.** Prove where personal data physically sits and block or alert on transfers that breach cross-border rules, with evidence a regulator will accept.
**ICP.** Japanese enterprises using global SaaS and cloud; multinationals handling Japanese personal data.
**Market.** `A24` — the April 2026 APPI amendment bill, and **administrative fines introduced for the first time**, expected by 2028. `A4` Japan cybersecurity $11.36B (2026).
**Wedge.** Fines convert a policy document into a budget line. The two-year lead time to 2028 is exactly the enterprise procurement cycle — start selling now.
**Rivals.** Microsoft Purview, OneTrust, Securiti, domestic consultancies.
**Pricing.** ¥8–40M/yr.
**Risk.** Fines are expected by 2028 but not yet in force; a buyer can defer, and many will.
**Call.** `BUILD` · Pull 4 · Moat 3 · TTR 4 — dated regulatory catalyst with a realistic runway.

#### 060 · Kaiji — DSAR automation for Japanese data subjects
**Pitch.** Receive, verify, fulfil and log data subject requests across a Japanese enterprise's systems, within statutory time limits.
**ICP.** Japanese enterprises with consumer data.
**Market.** `A24` APPI amendment.
**Wedge.** Modest. Japanese DSAR volumes remain low compared to GDPR jurisdictions, so the automation ROI is weak today.
**Rivals.** OneTrust, Securiti, Transcend.
**Pricing.** ¥3–15M/yr.
**Risk.** You are betting on a request-volume increase that has not happened and may not.
**Call.** `WATCH` · Pull 2 · Moat 2 · TTR 4 — right in principle, no volume yet.

#### 061 · Doui — consent lifecycle for APPI
**Pitch.** Capture, version and prove consent for each purpose of use, and enforce purpose limitation downstream in the data pipeline.
**ICP.** Japanese consumer businesses, especially those doing secondary data use.
**Market.** `A24`; `A39` Medical DX explicitly includes **expanded secondary use of medical data** — a purpose-limitation problem with a government mandate behind it.
**Wedge.** Purpose limitation enforced *in the pipeline* rather than recorded in a document is the difference between a consent tool and a consent control. Almost everyone ships the document.
**Rivals.** OneTrust, Securiti, domestic CMP vendors.
**Pricing.** ¥5–25M/yr.
**Risk.** Requires deep data-pipeline integration, which means long implementations.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 3 — the secondary-use mandate in `A39` is the opening.

#### 062 · Cleanroom — privacy-preserving analytics for regulated data
**Pitch.** Let two parties compute over combined datasets without either seeing the other's raw records — for health research, fraud consortia and advertising measurement.
**ICP.** Hospitals and pharma (see `A39` secondary use), banks in fraud consortia.
**Market.** `A39` Medical DX secondary use; `A3` digital health $491.62B (2026).
**Wedge.** Japan's national medical information platform creates a legitimate secondary-use pipeline that needs exactly this primitive.
**Rivals.** Snowflake/Databricks clean rooms, LiveRamp, Duality, Devron.
**Pricing.** $100–500k/yr.
**Risk.** The data-cloud vendors give clean rooms away as a platform feature; standalone is hard.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 2 — right primitive, wrong layer to sell at.

#### 063 · Notify — multi-jurisdiction breach notification workflow
**Pitch.** One incident, forty jurisdictions, forty different clocks and thresholds. Determine obligations, generate notifications, track deadlines, keep the audit log.
**ICP.** Multinational legal and privacy teams; incident response firms.
**Market.** `A24` APPI (Japan retains authority reporting and adds fines); `A11` ACD Law sectoral reporting; `A45` **770 HIPAA breaches in 2025, a record**.
**Wedge.** The obligation matrix is the asset and it must be maintained by lawyers, continuously. That maintenance burden is the moat — it is why this stays hard.
**Rivals.** RadarFirst, OneTrust, law firm services.
**Pricing.** $50–300k/yr.
**Risk.** Get a threshold wrong and your customer's regulator penalty is arguably your fault. Insurance and disclaimers matter here more than product.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 3 — maintenance-as-moat, with real liability attached.

#### 064 · Quantify — cyber risk quantification the underwriter accepts
**Pitch.** Translate a security posture into a loss distribution in currency, in a form an insurer will actually price against.
**ICP.** CISOs justifying budget; insurers underwriting.
**Market.** `A21` cyber insurance $23.29B (2026) → $44.67B (2032), and the proof point: **Zurich took a minority stake in Safe Security specifically to feed cyber-risk quantification into underwriting**. That is a strategic buyer validating the category with its balance sheet.
**Wedge.** Sell to the carrier, not the CISO. The carrier has a P&L reason to believe you and the CISO has a reason to game you.
**Rivals.** Safe Security (Zurich-backed), Kovrr, Axio, BitSight.
**Pricing.** $80–500k/yr; carrier deals larger.
**Risk.** Model credibility is everything and is nearly impossible to establish without loss data you do not have. The carriers do have it — hence: partner, don't compete.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — the Zurich/Safe deal is both the validation and the warning.

#### 065 · Warranty — continuous control attestation for insurance
**Pitch.** Continuously verify that the controls a policyholder attested to — MFA, EDR, offline backups, IR plan — are still actually in place, and feed that to the carrier.
**ICP.** Cyber insurers and MGAs; brokers.
**Market.** `A21` — **carriers now demand MFA, EDR, offline backups and IR plans as baseline conditions for coverage**, and combined ratios stay below 90% partly through technology-driven loss prevention.
**Wedge.** Attestation happens once a year on a form and decays immediately. Continuous verification lets the carrier price accurately and gives the insured a premium credit — both sides are paid to adopt it. That two-sided incentive is rare and valuable.
**Rivals.** Coalition and At-Bay (carriers doing it in-house), Corvus (Travelers).
**Pricing.** Per-policy $50–500/yr, carrier-paid.
**Risk.** The tech-forward carriers built this internally already; you are selling to the ones who didn't, which is a selection problem.
**Call.** `BUILD` · Pull 4 · Moat 3 · TTR 3 — two-sided incentive alignment is the strongest structural feature any concept in this section has.

#### 066 · Claim — cyber claims forensics acceleration
**Pitch.** Standardized evidence collection and loss substantiation for cyber claims, cutting the forensic bill and the settlement time.
**ICP.** Carriers, loss adjusters, breach counsel.
**Market.** `A21` cyber insurance.
**Wedge.** Forensics is the largest and least controlled line item in a cyber claim. Carriers want it capped.
**Rivals.** Panel forensics firms (Mandiant, Kroll, CrowdStrike Services) who are structurally opposed to this existing.
**Pricing.** Per-claim $2–20k.
**Risk.** You are attacking the revenue of the firms the carriers already trust, and those relationships are decades old.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 3 — correct economics, hostile channel.

#### 067 · Tenant — multi-tenant SOC copilot for MSSPs
**Pitch.** An analyst assistant built for the MSSP shape of the problem: 200 customers, 200 different stacks, one analyst on shift.
**ICP.** MSSPs and MDR providers, especially mid-sized regional ones.
**Market.** `A30` MDR $5.09B (2026) → $13.45B (2031) @21.45%; **~64% of enterprises already integrate AI analytics into security operations** and MDR automated triage reportedly cuts false positives ~39%.
**Wedge.** Sell to the provider, not the enterprise. One MSSP deal delivers 200 logos of coverage, and MSSPs buy on gross-margin maths rather than feature checklists — a faster, more rational sale.
**Rivals.** Dropzone, Radiant, Prophet Security, Torq, and every MDR building in-house.
**Pricing.** Per-analyst-seat or per-alert; $50–400k/yr.
**Risk.** MSSP margins are thin and they will squeeze you at every renewal.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — the channel-first framing is what makes this beat the direct-to-enterprise crowd.

#### 068 · Warrant — alert triage with a defensible reasoning trail
**Pitch.** An autonomous triage agent that shows every piece of evidence it consulted, everything it could not retrieve, and how much of each source it actually read.
**ICP.** SOC teams that have been burned by an AI tool confidently closing a real incident.
**Market.** `A30` MDR; `A2` threat intelligence.
**Wedge.** This is the security-operations version of this repository's exhaustiveness rule. Every triage agent claims accuracy; none makes its *blind spots* first-class output. "I could not reach the EDR API for host X, so this verdict excludes endpoint evidence" is the single most valuable sentence an AI SOC tool can emit, and almost none of them emit it.
**Rivals.** Dropzone, Radiant, Intezer, Microsoft Security Copilot.
**Pricing.** $60–400k/yr.
**Risk.** Honesty about limitations demos worse than confident wrongness. You will lose bake-offs to tools that overclaim.
**Call.** `BUILD` · Pull 4 · Moat 3 · TTR 4 — differentiated on exactly the axis buyers learn to care about after their first bad incident.

#### 069 · Detections — detection engineering as code
**Pitch.** Version-controlled, tested, portable detection rules with CI, coverage mapping to ATT&CK, and cross-SIEM compilation.
**ICP.** Mature detection engineering teams; MSSPs.
**Market.** `A30` MDR; `A2` threat intelligence.
**Wedge.** Portability across SIEMs is genuinely valuable during a migration and worth little otherwise — and migrations are rare.
**Rivals.** Panther, Anvilogic, SOC Prime, Sigma (free standard), Detection-as-code repos.
**Pricing.** $40–200k/yr.
**Risk.** Sigma is free and good; the buyer population is small and unusually capable of building it themselves.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 4 — sophisticated buyers are the worst buyers for tooling like this.

#### 070 · Rehearse — breach and attack simulation with business context
**Pitch.** Continuously validate that controls stop the attacks that matter to *this* organization, mapped to the business processes at risk.
**ICP.** Enterprises with mature control stacks.
**Market.** `A49` CTEM $1.48B (2026) → $4.22B (2036) @11.0%; `A5` ASM vendor list includes Cymulate.
**Wedge.** Business-process mapping instead of technique coverage percentages.
**Rivals.** Cymulate, SafeBreach, AttackIQ, Pentera, Picus.
**Pricing.** $60–350k/yr.
**Risk.** Five well-funded incumbents, single-digit differentiation, and an 11% market CAGR.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 3 — a full category with no room.

#### 071 · Hypothesis — threat hunting hypothesis generation
**Pitch.** Turn new threat intelligence into environment-specific hunt queries automatically, and track which hunts were run and what they found.
**ICP.** Threat hunting teams at large enterprises.
**Market.** `A2` threat intelligence $11.55B (2025) → $22.97B (2030).
**Wedge.** Hunt coverage tracking — knowing what you have *not* looked for — is the underserved half.
**Rivals.** Recorded Future, Anvilogic, in-house.
**Pricing.** $50–250k/yr.
**Risk.** Only large, mature teams hunt; that is a few hundred buyers globally.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 4 — tiny addressable buyer set.

#### 072 · Runbook — incident response orchestration for the mid-market
**Pitch.** Pre-built, maintained IR runbooks that execute against a mid-market stack, for organizations with no IR team at all.
**ICP.** 200–2,000 employee companies; their MSPs.
**Market.** `A30` MDR $5.09B (2026) @21.45%; `A48` — **88% of organizations suffered a security event attributable to a skills shortage**.
**Wedge.** The mid-market cannot hire the people (`A48`) and cannot afford enterprise SOAR. Packaged runbooks are the substitute for headcount.
**Rivals.** Torq, Tines, Blink, and MDR providers who fold IR into the service.
**Pricing.** $15–80k/yr.
**Risk.** MDR bundles IR, and the mid-market would rather buy the outcome than the tool.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 4 — the buyer prefers a service, and services will win this.

#### 073 · Tabletop — automated incident exercises
**Pitch.** Scenario-driven tabletop exercises with role-specific injects, scoring and a board-ready readout, run quarterly instead of annually.
**ICP.** Enterprises with board-level cyber oversight duties; `A11`-designated operators.
**Market.** `A11` ACD Law raises board attention in Japan; `A48` skills gap.
**Wedge.** Board reporting is the deliverable. The exercise is the excuse.
**Rivals.** Immersive Labs, Cyber Ranges, consultancies.
**Pricing.** $25–120k/yr.
**Risk.** Training and exercise budgets are exactly what `A48` says got cut when budget replaced talent as the top constraint.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 5 — the workforce data argues against it.

#### 074 · Restore — ransomware recovery readiness testing
**Pitch.** Continuously prove that backups actually restore — full application-level recovery tests in an isolated environment, with a time-to-recover number.
**ICP.** Any organization with a recovery-time objective it has never tested. Healthcare first.
**Market.** `A45` — **ransomware drives 48% of confirmed breaches; healthcare averaged 2.3 attacks/day in H1 2026; average healthcare breach $7.42M**; and the fact that reframes the sale entirely: **in-hospital mortality rises 34–38% for patients admitted when an attack begins**. `A21` — carriers now require offline backups as a coverage condition.
**Wedge.** Backup vendors report job success. Nobody proves *restoration of a working application within the RTO*, which is the only thing that matters at 3am. The gap between "backup succeeded" and "hospital is running" is where people die, per `A45`.
**Rivals.** Rubrik, Veeam, Commvault (all report on their own backups — a conflict of interest), Cohesity.
**Pricing.** $50–300k/yr.
**Risk.** The backup vendors will claim they already do this. They mostly do not, but the buyer cannot easily tell.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — the mortality statistic in `A45` makes this the most morally clear-cut product in this document.

#### 075 · Immutable — backup integrity and air-gap verification
**Pitch.** Independently verify that "immutable" backups are genuinely immutable and that air gaps are genuinely gapped.
**ICP.** Same as #074.
**Market.** `A45`; `A21` insurance control conditions.
**Wedge.** Independence from the backup vendor is the entire value; auditors and insurers both prefer a second pair of eyes.
**Rivals.** Backup vendors' own attestations; Index Engines.
**Pricing.** $30–150k/yr.
**Risk.** Narrow — likely a feature of #074 rather than a company.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — fold into #074.

#### 076 · Passive — OT asset discovery without touching the network
**Pitch.** Build a complete OT asset inventory from passive traffic observation only, because active scanning in a plant is how you stop a production line.
**ICP.** Manufacturing, utilities, and the `A11` critical-infrastructure sectors.
**Market.** `A15` OT security $27.39B (2026) → $58.94B (2031) @16.6%; ICS $20.55B (2026). `A11` puts Japanese operators under new obligations from 2026-10-01.
**Wedge.** Japan's manufacturing base is enormous, its OT estates are old, and `A11` just made their security a reportable matter. Domestic-language OT tooling barely exists.
**Rivals.** Claroty, Nozomi, Dragos, Armis, Tenable OT — all strong, all foreign, all weak on Japanese-language support and local integrator relationships.
**Pricing.** ¥10–60M/yr per site.
**Risk.** The incumbents are technically excellent; your only real edge is localization and channel.
**Call.** `WEDGE` · Pull 5 · Moat 2 · TTR 3 — sell the channel and the language, not the technology.

#### 077 · Protocol — OT anomaly detection with process awareness
**Pitch.** Detect not just unusual network traffic but physically implausible process states — a valve command sequence that no legitimate operation would issue.
**ICP.** Chemical, power, water, heavy manufacturing.
**Market.** `A15` OT/ICS.
**Wedge.** Process-model awareness is a genuinely higher bar than protocol parsing, and it is what separates an alert from an incident.
**Rivals.** Dragos (strongest here), Claroty, Nozomi.
**Pricing.** ¥20–100M/yr.
**Risk.** Requires deep per-process engineering; it does not generalize across verticals, so every new vertical is a new company.
**Call.** `WATCH` · Pull 4 · Moat 4 · TTR 2 — high moat, terrible scaling economics.

#### 078 · Facility — building management system security
**Pitch.** Secure the HVAC, access control, elevators and power management that sit on the corporate network and were installed by a contractor who left in 2011.
**ICP.** Commercial real estate, hospitals, data centres.
**Market.** `A15` OT security $27.39B (2026); `A6` healthcare cybersecurity $42.31B (2026) for the hospital slice.
**Wedge.** BMS is orphaned between facilities management and IT, so nobody owns the risk — which is why it stays unaddressed and why selling it is hard.
**Rivals.** Claroty, Armis, Johnson Controls / Siemens (the vendors themselves).
**Pricing.** $30–200k/yr.
**Risk.** No budget owner means no purchase order, however real the risk is.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 3 — find the budget owner before writing code.

#### 079 · Port — maritime and port OT security
**Pitch.** Security monitoring for terminal operating systems, crane control networks and vessel systems.
**ICP.** Port authorities, terminal operators, shipping lines.
**Market.** `A15` OT security; `A11` — transport is among the 15 designated Japanese critical-infrastructure sectors.
**Wedge.** Japan's ports are critical infrastructure under a law with a date on it, and port OT is notoriously antique. A 2023 ransomware incident at a major Japanese port made this concrete for the buyer.
**Rivals.** Naval Dome, Dragos, Claroty, the classification societies.
**Pricing.** ¥30–150M per port.
**Risk.** Tiny number of buyers, each with glacial public-sector procurement.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 2 — few customers, but each is large and sticky.

#### 080 · Fleet — connected vehicle security operations
**Pitch.** A SOC purpose-built for vehicle fleets: telematics anomaly detection, OTA update integrity, ECU-level incident response.
**ICP.** OEMs and tier-1 suppliers subject to UNECE R155/R156 cybersecurity management system requirements.
**Market.** `A15` OT security; `A18` SBOM — automotive is one of the few sectors where SBOM is genuinely mandated in practice.
**Wedge.** R155 requires a certified cybersecurity management system covering the whole vehicle lifecycle. Japanese OEMs are a concentrated, regulated, well-funded buyer set on the doorstep.
**Rivals.** Upstream Security, Argus (Elektrobit), VicOne, C2A.
**Pricing.** $500k–5M per OEM programme.
**Risk.** Multi-year sales cycles and OEMs that prefer to build in-house or buy the supplier outright.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 2 — long, slow, large, and Japan-advantaged.

#### 081 · Orbit — satellite ground segment security
**Pitch.** Security monitoring for the ground stations, TT&C links and mission control systems of commercial satellite operators.
**ICP.** Satellite operators, launch providers, national space agencies.
**Market.** `A15` OT security; `A25` geospatial as the adjacent buyer.
**Wedge.** Real and underserved; the ground segment is where satellites actually get attacked.
**Rivals.** SpiderOak, Xage, primes' internal teams.
**Pricing.** $200k–2M.
**Risk.** A handful of buyers worldwide, most of them defence-adjacent with clearance requirements you cannot meet as a startup.
**Call.** `WATCH` · Pull 3 · Moat 4 · TTR 2 — a great problem attached to almost no market.

#### 082 · Inventory — API discovery from traffic
**Pitch.** Find every API a company actually exposes — including the ones not in any spec, gateway or documentation — by observing traffic.
**ICP.** Enterprises with unmanaged API sprawl; financial services first.
**Market.** `A35` API security $1.195B (2026) at ~28.46% CAGR. Vendors named: Salt Security, Kong, Microsoft, AWS; Akamai partnered with Apiiro.
**Wedge.** Shadow and zombie APIs are the actual exposure and by definition are absent from every inventory you could ask for.
**Rivals.** Salt Security, Noname (Akamai), Traceable, 42Crunch.
**Pricing.** $60–350k/yr.
**Risk.** Consolidating fast — Noname went to Akamai, and the CDN/WAF vendors are absorbing the category wholesale.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 4 — the consolidation is already underway.

#### 083 · Logic — API business-logic abuse detection
**Pitch.** Detect abuse that is technically valid: enumeration through a legitimate endpoint, price manipulation, workflow sequence violations.
**ICP.** E-commerce, fintech, ticketing.
**Market.** `A35` API security; `A44` — **$20.9B total cybercrime losses in 2025 (+26% YoY), with nearly 85 cents of every lost dollar going to cyber-enabled fraud rather than malware or zero-days**.
**Wedge.** That `A44` breakdown is the strategic insight of this whole section: the money is being lost to *legitimate-looking actions*, not exploits. Almost the entire security industry is instrumented for the other 15 cents.
**Rivals.** Traceable, Salt, Cequence, DataDome, Kasada.
**Pricing.** $80–500k/yr.
**Risk.** Business-logic rules are per-customer, which makes onboarding a services engagement in disguise.
**Call.** `BUILD` · Pull 5 · Moat 4 · TTR 3 — follows directly from the strongest single datapoint in this research.

#### 084 · Takeover — account takeover defence tuned for credential stuffing
**Pitch.** Detect and stop ATO using leaked-credential intelligence combined with behavioural signals, before the fraud rather than after.
**ICP.** Consumer platforms, banks, telcos.
**Market.** `A10` — **6.8B stolen credentials traded in 2025 (+22% YoY); 73% appear within 48 hours; organizations with proactive dark-web monitoring reported a 67% reduction in ATO incidents**. `A44` 28.6M phished identity records recaptured in 2025.
**Wedge.** That 48-hour window is the product. Intelligence delivered on day 30 is archaeology.
**Rivals.** SpyCloud, Arkose, DataDome, HUMAN, Castle.
**Pricing.** $50–500k/yr by MAU.
**Risk.** Requires a credential corpus you must either buy or build, and building it means operating in places with legal complexity.
**Call.** `WEDGE` · Pull 5 · Moat 3 · TTR 3 — strong data, but the corpus is the barrier to entry and you are on the wrong side of it at day one.

#### 085 · Wire — payment fraud interception at the approval step
**Pitch.** Sit at the moment of payment approval and verify counterparty, bank details and change history against everything known — the last checkpoint before the money leaves.
**ICP.** Finance departments; AP teams; mid-market and up.
**Market.** `A44` — **FBI IC3 logged 24,768 BEC complaints and $3.05B in losses in 2025; $55.5B over a decade; average loss $137,000 per incident.**
**Wedge.** Email security tries to stop the message. That fails often enough that $3.05B still walks out the door annually. Intercepting at the *payment approval* step in the ERP is a second, independent line of defence with a much clearer ROI story: one prevented incident pays for a decade.
**Rivals.** Trustpair, nsKnox, Bottomline, Tipalti (partial).
**Pricing.** $30–200k/yr, easily justified against a $137k average loss.
**Risk.** ERP integration depth (SAP, Oracle, and in Japan, OBIC and Bizsoft) is the real work.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 3 — the clearest single-statistic ROI case in this document.

#### 086 · Relationship — vendor email compromise detection
**Pitch.** Model the historical email relationship with each vendor so that a subtle change — new domain, new signatory, new bank detail, changed writing style — is flagged even when the account is genuinely the vendor's.
**ICP.** Any company that pays invoices by email.
**Market.** `A44` BEC; `A8` TPRM.
**Wedge.** Vendor email compromise defeats every control that assumes the sender is the attacker. Here the sender is the real vendor, genuinely compromised — so only relationship history catches it.
**Rivals.** Abnormal Security (strong), Proofpoint, Mimecast, Microsoft Defender for Office.
**Pricing.** $25–150k/yr.
**Risk.** Abnormal has this well covered and enormous distribution.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 4 — right problem, taken.

#### 087 · Departure — offboarding-window exfiltration detection
**Pitch.** Watch the specific, narrow, predictable window around resignation and layoff for data movement to personal cloud, removable media and AI tools.
**ICP.** Enterprises with high attrition or ongoing restructuring; IP-heavy R&D organizations.
**Market.** `A38` insider risk $4.5–7.09B (2026), average annualized cost **$19.5M** (Ponemon/DTEX 2026), and the datapoint that defines the product: **a 720% surge in exfiltration activity in the 24 hours before a layoff**, with personal cloud at 22.7%, removable media at 15.6% and genAI tools at 13.1% of incidents.
**Wedge.** Continuous surveillance of all employees is expensive, legally fraught and culturally corrosive — and in Japan or the EU, close to unsellable. Monitoring a *known, bounded, HR-triggered window* is proportionate, defensible under works-council and APPI scrutiny, and per that 720% figure is where nearly all the risk actually lives. Constraint as feature.
**Rivals.** DTEX, Code42 (Mimecast), Proofpoint ITM, Varonis.
**Pricing.** $40–250k/yr.
**Risk.** Requires an HR system integration to know the window, which means a second stakeholder in every deal.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — narrowing the scope is what makes it both effective and legally saleable.

#### 088 · Proportionate — privacy-preserving insider risk for works-council jurisdictions
**Pitch.** Insider risk detection that produces aggregate risk signals without individual-level surveillance, designed to pass a European works council or a Japanese labour-union review.
**ICP.** Multinationals blocked from deploying US-style insider tooling in EU and Japanese entities.
**Market.** `A38` insider risk; `A24` APPI purpose limitation.
**Wedge.** US insider-risk tools are frequently undeployable in EU and Japanese subsidiaries. That is not a localization gap, it is a design gap, and the enterprises affected are large.
**Rivals.** Nobody is designing for this constraint first. The incumbents bolt on consent screens.
**Pricing.** $50–300k/yr.
**Risk.** Aggregate-only signals genuinely detect less. You must be honest that this is a trade, not a free lunch.
**Call.** `WEDGE` · Pull 3 · Moat 4 · TTR 3 — a real unserved constraint, and a rare case where regulation creates a product rather than a cost.

#### 089 · Kami — document and print channel leakage
**Pitch.** Track sensitive-document printing, scanning and physical handling — the paper channel that DLP ignores.
**ICP.** Japanese enterprises, law firms, government contractors.
**Market.** `A38` insider risk; `A24` APPI.
**Wedge.** Japanese corporate process remains unusually paper-intensive, and multifunction printers are a genuine, ignored exfiltration path. Ricoh, Fuji Xerox and Canon MFPs are everywhere and their logs are underused.
**Rivals.** MFP vendors' own management suites; domestic DLP.
**Pricing.** ¥5–30M/yr.
**Risk.** The MFP vendors own the hardware, the channel and the logs, and can close this whenever they choose.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 4 — culturally astute, structurally weak.

#### 090 · Continuous — PTaaS for Japanese regulated firms
**Pitch.** Continuous penetration testing delivered in Japanese, with reports in the format Japanese regulators and internal audit committees expect.
**ICP.** Japanese financial institutions and `A11` critical-infrastructure operators.
**Market.** `A32` PTaaS $0.72B (2026) → $1.98B (2031) @22.6%; leaders Veracode and Synack. `A4` Japan cybersecurity $11.36B (2026).
**Wedge.** Annual pentest-by-consultancy is the Japanese norm and it does not survive contact with continuous deployment. But the buyer will only switch if the deliverable still looks like the report their audit committee has approved for fifteen years. Match the artefact, change the engine.
**Rivals.** Synack, Cobalt, HackerOne, domestic security consultancies (LAC, NRI Secure).
**Pricing.** ¥10–50M/yr.
**Risk.** Japanese firms' preference for named, in-person, long-standing consultancies is a deep cultural moat *against* you.
**Call.** `WEDGE` · Pull 4 · Moat 2 · TTR 3 — the report format is the trojan horse.

#### 091 · Answer — security questionnaire automation, answering side
**Pitch.** Answer inbound security questionnaires automatically from a maintained evidence base, with confidence scores and a human review queue for anything uncertain.
**ICP.** B2B SaaS vendors drowning in customer security reviews.
**Market.** `A27` GRC platforms $23.32B (2026); **60–70% of first-time SOC 2 organizations already use Vanta, Drata, Secureframe or Sprinto**, and **over 70% of enterprise buyers require SOC 2**. `A8` TPRM $10.60B (2026).
**Wedge.** None left. This is a headline feature of all four named incumbents, and they have the compliance data already.
**Rivals.** Vanta, Drata, Secureframe, Sprinto, Conveyor.
**Pricing.** $10–50k/yr.
**Risk.** You would be entering a four-way funded fight to sell an existing feature.
**Call.** `AVOID` · Pull 4 · Moat 1 · TTR 5 — high demand, zero available position.

#### 092 · Kanshi — continuous compliance for Japanese frameworks
**Pitch.** Evidence automation for the frameworks Japanese enterprises actually get audited against — ISMS/JIS Q 27001, the FSA's financial guidelines, PMS/PrivacyMark — rather than SOC 2.
**ICP.** Japanese enterprises and their auditors.
**Market.** `A27` GRC platforms $23.32B (2026), eGRC $57.10B (2026) @10.8%. `A24` APPI fines expected by 2028.
**Wedge.** Vanta and Drata are built around SOC 2 and ISO 27001 because that is what US buyers demand. PrivacyMark and the FSA guidelines are what Japanese buyers are actually audited against, and no compliance-automation vendor has done that mapping work properly.
**Rivals.** Vanta and Drata (entering Japan), domestic consultancies doing it manually.
**Pricing.** ¥3–20M/yr.
**Risk.** Vanta localizing is a real and foreseeable threat; your lead time is maybe 24 months.
**Call.** `BUILD` · Pull 4 · Moat 3 · TTR 4 — a genuine framework gap, but a closing window.


## Part C — Health & medical (093–150)

> A warning that applies to this entire section. The digital health anchor `A3`
> is quoted between **$420B and $492B for 2026** — but those figures include
> hardware, services and telehealth consultations. The *software* subsegments
> are one to three orders of magnitude smaller: precision medicine software is
> **$81M** (`A47`), pharmacovigilance software **$234.73M** (`A36`), AI in
> radiology **$600.8M** (`A17`). A digital health TAM slide is the most
> misleading artefact in venture capital. Every study below cites the narrowest
> honest anchor available, and several are deliberately unflattering as a
> result.

#### 093 · Karte — ambient clinical documentation in Japanese
**Pitch.** Ambient capture of the consultation, producing a Japanese clinical note in the structure Japanese physicians and EMRs expect (SOAP in Japanese, with the kanji-heavy terminology and the honorific register intact).
**ICP.** Japanese hospitals and clinics; the EMR vendors as a channel.
**Market.** `A9` genAI clinical documentation $1.05B (2026) → $10.50B (2034) @33.3%; US medical scribing alone $621.66M (2026). `A7` Japan digital health $29.2B (2024) → $55.8B (2033); **Japan EMR only $494.83M (2024) → $853.55M (2033) @6.3%** — note how small and slow-growing the Japanese EMR market is. *SAM:* if scribing reaches 20% of Japanese EMR spend, ~$170M by 2033. My arithmetic.
**Wedge.** Japanese medical language is not a localization task. Mixed kanji/kana/romaji drug names, the distinct 主訴/現病歴 note structure, and dictation habits shaped by a fee-for-service system make transplanted English models perform badly. Abridge and Nuance DAX are not coming for this soon.
**Rivals.** Abridge, Nuance DAX, Ambience; domestically, the EMR incumbents (Fujitsu, NEC, SoftBank/Ubie-adjacent).
**Pricing.** ¥30–80k per physician per month.
**Risk.** Japanese EMR vendors control the integration point and have every incentive to build rather than partner. Getting write access to the karte is the whole game.
**Call.** `BUILD` · Pull 5 · Moat 4 · TTR 3 — the language barrier that keeps US vendors out is the moat, and physician documentation burden is universal.

#### 094 · Kiroku — nursing care record automation
**Pitch.** Voice and sensor-driven documentation for elderly care facilities, producing the records required for long-term care insurance reimbursement without the caregiver touching a keyboard.
**ICP.** Japanese nursing homes and home-care providers.
**Market.** `A20` — Japan elderly care tech $1.18B (2025) → $3.76B (2033) @15.59%; services $11.77B (2024). And the structural driver: **2.4M caregivers needed in FY2026 against a 250,000 shortfall, widening to 570,000 by FY2040, with a job-openings-to-applicants ratio of 3.6.** The source notes explicitly that the biggest productivity gains may come from *removing administrative work* rather than replacing caregivers.
**Wedge.** That last point is the entire thesis, and it comes from the research rather than from me. Care robots are capital-intensive and slow; documentation time is recoverable immediately with software.
**Rivals.** Wiseman, NDソフトウェア, CareKarte, Blueship — domestic incumbents with the reimbursement-form logic already encoded.
**Pricing.** ¥30–100k per facility per month.
**Risk.** Reimbursement forms change with each LTC insurance revision cycle, which is a permanent maintenance tax.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — the demographic driver is arithmetic, not a forecast.

#### 095 · Keikaku — care plan drafting for care managers
**Pitch.** Draft the individualized care plan (ケアプラン) from assessment data, for the care manager to review and sign.
**ICP.** Japanese care management offices (居宅介護支援事業所).
**Market.** `A20` elderly care tech $1.18B (2025) @15.59%.
**Wedge.** Care plan authoring is a licensed professional's bottleneck and is highly templated in practice — a good fit for drafting-with-review.
**Rivals.** The same domestic care-software incumbents as #094.
**Pricing.** ¥15–40k per care manager per month.
**Risk.** Regulatory acceptance of AI-drafted care plans is unsettled; the professional body may resist.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — pair with #094 rather than standing alone.

#### 096 · Tentou — fall risk prediction in residential care
**Pitch.** Predict resident falls from gait, sleep, toileting and medication changes captured by ambient sensors, and prompt a specific intervention.
**ICP.** Nursing homes; their insurers.
**Market.** `A20` Japan elderly care tech; `A13` remote patient monitoring $30.9–67.3B (2026) — a **2.2× spread**, treat as directional.
**Wedge.** Falls are the dominant cause of the injury spiral that ends independent living, and the intervention is cheap if the timing is right.
**Rivals.** SafelyYou, Vayyar, Aisin/Paramount Bed (Japan), Konica Minolta HitomeQ.
**Pricing.** ¥3–10k per bed per month.
**Risk.** Sensor hardware means capex, installation and a much harder sale than pure software.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — strong need, hardware drag.

#### 097 · Shift — caregiver scheduling under a structural shortage
**Pitch.** Optimize caregiver rosters against qualification requirements, statutory ratios, commute time and stated preferences — the last of which is what actually reduces turnover.
**ICP.** Japanese care providers and hospitals.
**Market.** `A20` — a 3.6 job-openings-to-applicants ratio means retention, not recruitment, is the lever.
**Wedge.** Optimizing for staff preference rather than pure cost is counterintuitive to the buyer and is the thing that reduces the shortfall they actually feel.
**Rivals.** Domestic shift tools, generic workforce management, spreadsheets (the real incumbent).
**Pricing.** ¥50–200k per facility per month.
**Risk.** Cost-focused buyers will not pay for a retention benefit they cannot attribute for 18 months.
**Call.** `WEDGE` · Pull 4 · Moat 2 · TTR 4 — prove retention on their own historical data or lose the sale.

#### 098 · Ward — connected medical device inventory and risk
**Pitch.** Passive discovery and risk scoring of every connected medical device in a hospital, with clinical context (is it life-supporting? is it in use right now?).
**ICP.** Hospital IT and biomedical engineering.
**Market.** `A6` medical device security $8.05–8.30B (2026) → $22.69B (2034) @13.39%; healthcare cybersecurity $42.31B (2026). `A45` — **99% of hospitals have vulnerable devices**.
**Wedge.** IT security tools do not know that the device they want to patch is currently keeping someone alive. Clinical context turns an unusable finding into an actionable one.
**Rivals.** Medigate (Claroty), Armis, Ordr, Cynerio, Asimily.
**Pricing.** $50–300k/yr per hospital system.
**Risk.** A well-served category with five credible vendors; late entry needs a real differentiator.
**Call.** `WATCH` · Pull 5 · Moat 2 · TTR 3 — enormous pull, crowded field.

#### 099 · Downtime — clinical continuity during a cyber incident
**Pitch.** Keep the hospital clinically operational while the EMR is down: pre-staged offline patient summaries, paper-equivalent workflows, medication lists, and a rehearsed switchover.
**ICP.** Hospital COOs and chief nursing officers — not the CISO.
**Market.** `A45` — **healthcare averaged 2.3 ransomware attacks per day in H1 2026 (410 attacks, 247 on direct-care providers, +14% vs H2 2025); average breach $7.42M; Change Healthcare affected 192.7M individuals**; and decisively: **in-hospital mortality rises 34–38% for Medicare patients already admitted when an attack begins**.
**Wedge.** Every dollar in healthcare cyber goes to prevention. That mortality figure says prevention has already failed often enough to kill people, and almost nobody sells *clinical* resilience for the days after. This is a different budget (clinical operations), a different buyer (CNO), and an uncontested position.
**Rivals.** Consultancies selling downtime procedures as a binder; no real software vendor.
**Pricing.** $80–400k/yr per health system.
**Risk.** Requires continuous, safe extraction of clinical data to a standby system — technically demanding and a regulatory conversation of its own.
**Call.** `BUILD` · Pull 5 · Moat 4 · TTR 3 — the strongest health concept in this document, and the one with a body count behind the business case.

#### 100 · Patchable — medical device vulnerability and SBOM management
**Pitch.** Ingest manufacturer SBOMs and advisories for the device fleet, correlate to what is deployed, and tell the biomed team which of 4,000 findings matter.
**ICP.** Hospital biomedical engineering; device manufacturers on the supply side.
**Market.** `A6` medical device security $8.30B (2026) → $22.69B (2034) @13.39%; `A18` SBOM management → $9.7B (2035) @13.2%. Device SBOM is one of the few genuinely enforced SBOM mandates.
**Wedge.** The consumption side again (see #052), but here the mandate is real and the buyer is identifiable.
**Rivals.** Finite State, MedCrypt, Asimily, Cybellum.
**Pricing.** $40–250k/yr.
**Risk.** MedCrypt and Finite State have the manufacturer relationships that make the data flow work.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — go to the manufacturers, not the hospitals.

#### 101 · Chart Sprawl — PHI discovery across hospital storage
**Pitch.** Find protected health information where it should not be: research shares, departmental drives, retired systems, personal cloud folders.
**ICP.** Hospital privacy officers and CISOs.
**Market.** `A6` healthcare cybersecurity; `A26` DSPM $2.20B (2025) → $6.19B (2033); `A45` **770 HIPAA breaches in 2025, a record year**.
**Wedge.** Hospitals have unusually chaotic unstructured storage because research and clinical operations have different rules and share infrastructure.
**Rivals.** Varonis, Cyera, BigID, Microsoft Purview.
**Pricing.** $60–300k/yr.
**Risk.** Generic DSPM does most of this; the healthcare specificity is thinner than it appears.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 4 — a vertical skin on a horizontal product.

#### 102 · Business Associate — healthcare vendor risk with clinical impact scoring
**Pitch.** Score the hospital's vendors not by generic security posture but by what happens clinically if they go down — the Change Healthcare lesson, operationalized.
**ICP.** Health system risk and procurement.
**Market.** `A8` TPRM $10.60B (2026) @14.34%, third-party breaches **+54%**; `A45` **Change Healthcare: 192.7M individuals from a single vendor compromise**.
**Wedge.** Change Healthcare proved that vendor concentration risk in healthcare is systemic, not incidental. Nobody maps *clinical* dependency; they map data access.
**Rivals.** Censinet (healthcare-specific), SecurityScorecard, Panorays.
**Pricing.** $60–300k/yr.
**Risk.** Censinet already owns the healthcare TPRM niche in the US.
**Call.** `WEDGE` · Pull 5 · Moat 3 · TTR 3 — the clinical-dependency framing is the only open angle.

#### 103 · Segment — isolation for unpatchable legacy devices
**Pitch.** Automatically generate and enforce network micro-segmentation policy for devices that can never be patched, without breaking the clinical workflow that depends on them.
**ICP.** Hospital IT.
**Market.** `A6` medical device security; `A45` 99% of hospitals have vulnerable devices.
**Wedge.** Segmentation is universally recommended and rarely done because getting it wrong takes clinical systems offline. Automating it safely is the hard, valuable part.
**Rivals.** Claroty, Armis, Illumio, Cisco.
**Pricing.** $80–400k/yr.
**Risk.** High blast radius; a segmentation mistake in a hospital is a patient-safety event.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 2 — genuinely hard, which is why it is defensible and slow.

#### 104 · Drift Clinical — post-deployment monitoring of clinical AI
**Pitch.** Monitor deployed clinical AI models for performance drift across patient subgroups, with alerting when real-world performance diverges from the validation set.
**ICP.** Hospitals running multiple AI tools; device manufacturers with post-market obligations.
**Market.** `A43` — **PMDA updated its core SaMD guidance on 2026-06-05 with new AI/ML validation requirements**, and its expert committee has published recommendations for reviewing **adaptive AI devices that change performance after marketing**. `A17` AI medical imaging $2.16B (2026) @30.7%. `A12` AI governance.
**Wedge.** Adaptive AI creates a post-market surveillance obligation that literally cannot be met with periodic manual review. The regulator has said so. That is a mandate looking for a product.
**Rivals.** Aidoc (own models), Ferrum Health, Vera/Duality-style monitoring, in-house.
**Pricing.** $60–300k/yr per health system; manufacturer deals larger.
**Risk.** Early — the adaptive-AI approval pathway is still being defined, per `A43`.
**Call.** `BUILD` · Pull 4 · Moat 4 · TTR 3 — the regulator has described the problem and not the solution. That is the best possible starting position.

#### 105 · Shinsa — SaMD regulatory submission support for Japan
**Pitch.** Structure and assemble PMDA SaMD submissions: classification determination, clinical evidence mapping, the new AI/ML validation and cybersecurity sections, in the required format.
**ICP.** Foreign digital health companies entering Japan; Japanese startups filing for the first time.
**Market.** `A43` — **PMDA updated SaMD guidance 2026-06-05**; **DASH for SaMD 2** is a five-year MHLW strategy explicitly designed to accelerate SaMD approval; **~100 AI SaMDs already approved and reimbursed in Japan**. `A7` Japan digital health.
**Wedge.** Japan is a large, reimbursed market that foreign digital health companies systematically skip because the regulatory path is opaque and Japanese-only. A firm that makes entry legible captures a stream of well-funded foreign entrants.
**Rivals.** Regulatory consultancies (expensive, unscalable), no software incumbent.
**Pricing.** $50–250k per submission; retainer model.
**Risk.** Fundamentally a consultancy until the templates are proven; regulatory judgement does not automate cleanly.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — start as a consultancy honestly, productize what repeats.

#### 106 · Ichigo — SaMD post-market surveillance automation
**Pitch.** Collect, triage and report real-world SaMD performance and adverse events to meet post-market obligations in Japan, the EU and the US simultaneously.
**ICP.** SaMD manufacturers selling in multiple jurisdictions.
**Market.** `A43`; `A36` pharmacovigilance software $234.73M (2026) — small, and the PV & drug safety software figure of $2.86B (2026) is the broader anchor.
**Wedge.** Multi-jurisdiction reporting from one evidence base; the obligations overlap but the formats do not.
**Rivals.** ArisGlobal, Oracle, IQVIA, RXLogix (named in `A36`) — all drug-focused, weak on device software.
**Pricing.** $80–400k/yr.
**Risk.** `A36` shows this is a genuinely small software market with entrenched vendors.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 3 — the anchor is honest and it is not large.

#### 107 · Triage — imaging worklist prioritization for shortage conditions
**Pitch.** Reorder the radiology worklist so the intracranial haemorrhage is read in 8 minutes and the routine knee waits, using AI detection as a triage signal rather than a diagnosis.
**ICP.** Hospital radiology departments; teleradiology providers.
**Market.** `A17` AI medical imaging $2.16B (2026) → $8.23B (2031) @30.7%; AI radiology $600.8M (2026) → $3.23B (2034) @23.38%. Growth is explicitly attributed to **rising imaging volumes against radiologist shortages**.
**Wedge.** Triage carries a much lower regulatory and liability burden than diagnosis while capturing most of the clinical benefit — the radiologist still reads everything, just in a better order.
**Rivals.** Aidoc (dominant), Viz.ai, Annalise.ai, Lunit, HeartFlow.
**Pricing.** $50–300k/yr per site.
**Risk.** Aidoc is well ahead with regulatory clearances and hospital relationships you cannot shortcut.
**Call.** `WATCH` · Pull 5 · Moat 2 · TTR 2 — enormous pull, a clear leader already.

#### 108 · Slide — digital pathology AI
**Pitch.** AI-assisted review of digitized pathology slides, prioritizing and pre-annotating cases.
**ICP.** Pathology labs, academic medical centres.
**Market.** `A17` AI medical imaging; `A47` precision oncology $133.06B (2026) @11.47%.
**Wedge.** Pathology digitization lags radiology by roughly a decade, which means the scanner installed base — not the algorithm — is the binding constraint.
**Rivals.** Paige, PathAI, Ibex, Proscia.
**Pricing.** $100–500k/yr per lab.
**Risk.** You are betting on someone else's hardware rollout schedule.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 2 — blocked by digitization, not by AI.

#### 109 · Naishikyo — endoscopy AI for gastric cancer screening
**Pitch.** Real-time lesion detection during upper GI endoscopy, tuned specifically for early gastric cancer.
**ICP.** Japanese hospitals and screening clinics; then Korea and China.
**Market.** `A17` AI medical imaging $2.16B (2026) @30.7%; `A43` — **~100 AI SaMDs approved and reimbursed in Japan**, with endoscopy among the established categories.
**Wedge.** Japan has the world's highest gastric cancer screening volume, the best endoscopist training, the deepest annotated image corpora, and — critically — an existing reimbursement pathway. This is a case where Japan is the *best* place to build, not merely a market to enter.
**Rivals.** Olympus (with AI built in), Fujifilm, AI Medical Service (Japanese, well funded), Cybernet.
**Pricing.** ¥2–10M per scope suite per year.
**Risk.** Olympus and Fujifilm control the endoscope hardware and are integrating AI natively. Independent software has a narrowing window.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 2 — genuine national advantage, hardware-vendor threat.

#### 110 · Retina — diabetic retinopathy screening at the point of care
**Pitch.** Autonomous retinal screening in primary care and pharmacy settings, referring only positives to ophthalmology.
**ICP.** Primary care networks, diabetes clinics, pharmacy chains.
**Market.** `A17` AI medical imaging; `A40` CGM $15.77B (2026) @15.09% as the adjacent diabetes-care budget.
**Wedge.** Moving screening out of ophthalmology entirely is the value; the AI is a means.
**Rivals.** Digital Diagnostics (IDx-DR), Eyenuk, Google/Verily.
**Pricing.** Per-screen $10–30.
**Risk.** Reimbursement for autonomous AI screening is established in the US and unclear elsewhere; camera capex again gates deployment.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — reimbursement clarity determines everything.

#### 111 · Fracture — emergency radiograph second read
**Pitch.** Automated second read on emergency department radiographs to catch the fractures missed at 3am.
**ICP.** Emergency departments, urgent care.
**Market.** `A17` AI radiology $600.8M (2026) @23.38%.
**Wedge.** Missed fracture is among the most common and most litigated diagnostic errors, which makes the malpractice-insurer a plausible payer.
**Rivals.** Gleamer, Annalise.ai, Aidoc, Radiobotics.
**Pricing.** Per-study $0.50–3.
**Risk.** Crowded, and per-study pricing puts you in a race to the bottom.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 3 — go via the insurer or don't go.

#### 112 · Echo — automated echocardiography measurement
**Pitch.** Automate the measurement and reporting burden in echocardiography, and enable acquisition guidance for non-expert operators.
**ICP.** Cardiology departments, point-of-care ultrasound users.
**Market.** `A17` AI medical imaging $2.16B (2026) @30.7%.
**Wedge.** Operator guidance expands *where* echo can be performed, which grows the market rather than splitting it.
**Rivals.** Us2.ai, Caption Health (GE), EchoNous, Philips.
**Pricing.** $30–150k/yr per department.
**Risk.** GE and Philips are bundling this into the ultrasound machine itself.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 3 — hardware vendors are absorbing it.

#### 113 · Skin — dermatology triage for primary care
**Pitch.** Lesion triage in primary care to decide who needs dermatology within two weeks and who needs reassurance.
**ICP.** Primary care networks, teledermatology providers.
**Market.** `A17` AI medical imaging; `A3` digital health.
**Wedge.** Weak. Performance on darker skin tones remains a documented, unsolved equity problem, and the liability from a missed melanoma is severe.
**Rivals.** SkinVision, DermaSensor, Google Health.
**Pricing.** Per-assessment $5–20.
**Risk.** Documented performance disparities across skin tones make this both an ethical and a legal exposure. Do not ship a triage tool that is worse for some patients without saying so loudly, and most of this market does not.
**Call.** `AVOID` · Pull 3 · Moat 2 · TTR 3 — the equity gap is not yet closed and the failure mode is fatal.

#### 114 · Nursing Note — ambient documentation for nurses
**Pitch.** Ambient documentation for nursing workflow — vitals, medication administration, care observations — rather than physician consultations.
**ICP.** Hospital nursing administration.
**Market.** `A9` genAI clinical documentation $1.05B (2026) → $10.50B (2034) @33.3%; `A20` Japan caregiver shortage as the parallel driver.
**Wedge.** Every ambient scribe vendor targets physicians because physicians have budget authority. Nurses are more numerous, document more, and are the actual staffing crisis — but the buying committee is different and slower.
**Rivals.** Nuance, Abridge and Ambience are all physician-first; little direct competition.
**Pricing.** $50–150 per nurse per month.
**Risk.** Nursing documentation is more structured and more legally sensitive than physician narrative; errors have direct patient-safety consequences.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 3 — the underserved half of a proven category.

#### 115 · Prior Auth — automated prior authorization
**Pitch.** Assemble and submit prior authorization requests automatically, with payer-specific criteria matching and appeal drafting.
**ICP.** US provider groups; specialty practices.
**Market.** `A33` — **AI in RCM $21.49B (2026) → $71.27B (2031) @27.1%**; >$500M invested in AI RCM in 2026 to date; **Candid Health raised a $120M Series D on 2026-07-22**.
**Wedge.** Little available. This is the single most funded problem in US healthcare software and the capital is already deployed.
**Rivals.** Cohere Health, Candid Health, Availity, Waystar, Infinitus.
**Pricing.** Per-transaction.
**Risk.** You would be entering against a market that just absorbed half a billion dollars in a single year.
**Call.** `AVOID` · Pull 5 · Moat 1 · TTR 4 — maximum pull, zero room. The `A33` funding data is a warning, not an invitation.

#### 116 · Denial — claim denial prediction before submission
**Pitch.** Predict which claims will be denied and why, and fix them before they go out.
**ICP.** Hospital revenue cycle teams.
**Market.** `A33` AI in RCM $21.49B (2026) @27.1%; RCM overall $95.22B (2026).
**Wedge.** Same crowding problem as #115, slightly less acute.
**Rivals.** Waystar, R1, Candid, AKASA.
**Pricing.** Gainshare on recovered revenue.
**Risk.** Well-capitalized incumbents with payer data you cannot obtain.
**Call.** `WATCH` · Pull 5 · Moat 2 · TTR 3 — good problem, late.

#### 117 · Rezeputo — Japanese claim (レセプト) pre-submission checking
**Pitch.** Validate Japanese medical claims against the fee schedule and審査 rules before submission to the review body, catching the errors that cause返戻 and查定.
**ICP.** Japanese hospitals and clinics; medical billing agents.
**Market.** `A7` Japan digital health, Japan EMR $494.83M (2024) → $853.55M (2033) @6.3%; `A39` Medical DX includes digitization of receipt and subsidy processes. `A33` AI-in-RCM globally @27.1% shows what this becomes.
**Wedge.** Japan's fee schedule (診療報酬点数表) is revised every two years, is extraordinarily intricate, and is a domain no foreign RCM vendor will ever enter. The `A33` gold rush is entirely US-focused; Japan's equivalent problem is unserved by comparison.
**Rivals.** Domestic レセコン vendors (Panasonic, Bizsoft, Nihon Iyaku), which handle submission but do relatively little predictive checking.
**Pricing.** ¥50–300k per facility per month.
**Risk.** The biennial fee schedule revision is a hard maintenance deadline you can never miss.
**Call.** `BUILD` · Pull 4 · Moat 5 · TTR 3 — the highest moat score in this section, precisely because the domain is impenetrable to outsiders.

#### 118 · Coding Audit — retrospective coding accuracy review
**Pitch.** Audit completed clinical coding for undercoding and compliance risk, with documentation improvement suggestions.
**ICP.** Hospital HIM departments.
**Market.** `A33` RCM $95.22B (2026).
**Wedge.** Thin; the RCM vendors already do this and the line between optimization and upcoding fraud is uncomfortably narrow.
**Rivals.** Optum, 3M/Solventum, AKASA, Nym.
**Pricing.** Gainshare.
**Risk.** An "optimize revenue" product in healthcare is one bad customer away from being an accessory to fraud. `A42` puts US fraud, waste and abuse at **$308.6B/yr** — you do not want to be measured as a contributor to it.
**Call.** `AVOID` · Pull 4 · Moat 1 · TTR 4 — the incentive alignment is wrong and the reputational tail risk is real.

#### 119 · Site Select — clinical trial site selection from real-world data
**Pitch.** Identify trial sites by actual patient population and historical enrolment performance rather than by investigator relationships.
**ICP.** Pharma clinical operations; CROs.
**Market.** `A22` decentralized clinical trials $10.74–14.29B (2026) @14.42%; **Medidata holds >13% share and the top five — Medidata, IQVIA, ICON, Parexel, Fortrea — hold 46%**.
**Wedge.** Enrolment failure is the largest single source of trial delay and it is largely a site-selection error made months earlier.
**Rivals.** IQVIA, Medidata, TriNetX, Citeline.
**Pricing.** $200k–2M per programme.
**Risk.** IQVIA owns both the data and the customer, which is a difficult combination to attack.
**Call.** `WATCH` · Pull 4 · Moat 3 · TTR 2 — the `A22` concentration figure (46% to five firms) explains why.

#### 120 · Enrol — patient recruitment for decentralized trials
**Pitch.** Find, screen and enrol trial participants directly, with eligibility pre-screening and remote consent.
**ICP.** Sponsors and CROs running decentralized or hybrid trials.
**Market.** `A22` DCT @14.42%; mHealth is 12.1% of the DCT technology market at 14.1% CAGR.
**Wedge.** Recruitment is paid per enrolled patient, which means outcome-aligned pricing is natural and the buyer's risk is low.
**Rivals.** Science 37, Curebase, Antidote, TrialSpark.
**Pricing.** Per enrolled patient, $500–5,000.
**Risk.** Several well-funded predecessors have struggled badly here; the unit economics are harder than they look.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 3 — a category with a poor track record.

#### 121 · Consent — electronic informed consent with comprehension testing
**Pitch.** eConsent that verifies the participant actually understood, not merely that they clicked.
**ICP.** Sponsors, IRBs, academic research centres.
**Market.** `A22` DCT.
**Wedge.** Comprehension verification is an ethics-committee priority and is essentially unaddressed by tools that only capture a signature.
**Rivals.** Medidata, Signant, Castor.
**Pricing.** $50–300k per study.
**Risk.** A feature of the eClinical suites, sold by the firms holding 46% share.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 4 — likely a module, not a company.

#### 122 · Signal — pharmacovigilance signal detection
**Pitch.** Detect safety signals across spontaneous reports, literature, claims and real-world data earlier than periodic aggregate review.
**ICP.** Pharma drug safety departments.
**Market.** `A36` — PV software **$234.73M (2026)**, PV & drug safety software $2.86B (2026) @12.8%. The research is explicit that the sector is moving **from rule-based case assessment toward AI-driven signal intelligence**.
**Wedge.** The direction of travel is stated by the market itself; the question is only whether an entrant or the incumbent gets there.
**Rivals.** IQVIA, Oracle, ArisGlobal, RXLogix (all named in `A36`).
**Pricing.** $200k–2M/yr.
**Risk.** Validated systems in pharma change vendors approximately never. Displacement cycles run 7–10 years.
**Call.** `WATCH` · Pull 4 · Moat 3 · TTR 2 — right direction, immovable incumbents.

#### 123 · Lit — automated literature monitoring for drug safety
**Pitch.** Continuous screening of global medical literature for adverse events attributable to a sponsor's products, in every language it is published in.
**ICP.** Pharma PV departments; smaller sponsors without a literature team.
**Market.** `A36` PV software $234.73M (2026).
**Wedge.** Non-English literature coverage, especially Japanese and Chinese journals, is genuinely weak in existing tools and is a compliance obligation regardless.
**Rivals.** ProPharma, IQVIA, Embase-based workflows.
**Pricing.** $50–300k/yr.
**Risk.** `A36` says the whole software market is $234.73M. This is a slice of a slice.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 4 — small but genuinely defensible on language coverage.

#### 124 · KOL — medical affairs expert mapping
**Pitch.** Map key opinion leaders by actual scientific influence — publications, trials, guideline authorship, referral networks — rather than by who attended last year's dinner.
**ICP.** Pharma medical affairs teams.
**Market.** `A28` AI drug discovery $5.00B (2026) is adjacent but not the buyer; medical affairs tooling had **no clean anchor**.
**Wedge.** Modest and well served.
**Rivals.** Veeva (dominant), H1, Definitive Healthcare.
**Pricing.** $100–500k/yr.
**Risk.** Veeva owns the pharma commercial stack completely.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 4 — Veeva.

#### 125 · Genjitsu — real-world evidence platform for Japan
**Pitch.** Assemble and query Japanese real-world data — claims (NDB), hospital DPC data, disease registries — for regulatory and HEOR use.
**ICP.** Pharma Japan affiliates, PMDA-facing regulatory teams, academic researchers.
**Market.** `A39` — the Medical DX programme explicitly includes **expanded secondary use of medical data** and a national medical information platform. `A7` Japan digital health $29.2B (2024) → $55.8B (2033).
**Wedge.** Japan's claims data is nationally comprehensive because of universal coverage — genuinely rarer and more complete than most countries' — and access is being formally widened by policy. Foreign RWE vendors have not built for it.
**Rivals.** IQVIA Japan, JMDC (domestic, strong), Milliman.
**Pricing.** $200k–2M/yr.
**Risk.** JMDC already holds the strongest Japanese claims position and knows it.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 2 — policy tailwind, entrenched domestic holder of the data.

#### 126 · Variant — genomic variant interpretation and reporting
**Pitch.** Interpret sequencing variants against current evidence and generate the clinical report, with automatic re-analysis when the evidence changes.
**ICP.** Clinical genetics labs, hospital molecular pathology.
**Market.** `A47` — genomics $22.6B (2026) → $72.5B (2033) @18.2%; NGS $9.8B (2026) @18.9%; **but precision medicine *software* is only $81M (2026)**. That gap is the story of this entry.
**Wedge.** Automatic re-analysis when classifications change is real clinical value that manual workflows cannot provide.
**Rivals.** SOPHiA GENETICS, Fabric Genomics, Congenica, Franklin (free tier).
**Pricing.** Per-sample $20–200.
**Risk.** `A47` is unusually clear that the software layer captures a tiny fraction of the sequencing value. The instrument and reagent vendors take the economics.
**Call.** `WATCH` · Pull 4 · Moat 3 · TTR 3 — the $81M software figure against a $22.6B market is the whole lesson.

#### 127 · Board — molecular tumour board decision support
**Pitch.** Assemble the full evidence picture for a tumour board — variants, trial eligibility, guideline pathways, prior therapy — into one reviewable case.
**ICP.** Cancer centres.
**Market.** `A47` precision oncology $133.06B (2026) @11.47%; genomics segment is **37% of oncology precision medicine in 2026**.
**Wedge.** Preparation for a tumour board consumes hours of a specialist's time per case and is almost entirely assembly work.
**Rivals.** Foundation Medicine (Roche), Tempus, Navify (Roche), Syapse.
**Pricing.** $100–500k/yr per centre.
**Risk.** Roche and Tempus give this away to drive their own testing volume — a classic razor-and-blade squeeze you cannot match.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 3 — bundled away by the test vendors.

#### 128 · Cascade — hereditary risk cascade screening
**Pitch.** When a pathogenic variant is found, systematically identify, contact and screen at-risk relatives — the step that is clinically mandated and almost never completed.
**ICP.** Genetics services, health systems, payers.
**Market.** `A47` genomics $22.6B (2026) @18.2%.
**Wedge.** Cascade screening completion rates are dismal, the clinical benefit is unusually well evidenced, and payers have a clear financial interest in prevention.
**Rivals.** ConnectMyVariant (non-profit), Invitae (bankrupt — a warning), Genome Medical.
**Pricing.** Payer-funded per-family $200–1,000.
**Risk.** Contacting relatives raises real privacy and consent complexity in every jurisdiction, and Invitae's collapse says this adjacency is harder than it looks.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 3 — strong clinical logic, difficult consent model.

#### 129 · Taisha — metabolic coaching on continuous glucose data
**Pitch.** Behavioural coaching driven by continuous glucose data for pre-diabetic and metabolic-syndrome populations, delivered through employers and insurers.
**ICP.** Japanese employers and health insurance societies (健康保険組合).
**Market.** `A40` — CGM $15.77B (2026) @15.09%; **OTC CGM $528.2M (2025) growing at 17.1%**, described as consumer metabolic-health demand rather than diabetes care. `A16` DTx $12.45B (2026).
**Wedge.** Japan's 特定健診 (specific health checkup) system means employers already hold metabolic risk data on every employee over 40 and are already obliged to act on it. That is a captive, funded, legally-motivated channel that does not exist in most countries — see #130.
**Rivals.** Levels, Signos, January AI; in Japan, CureApp and the insurer-affiliated programmes.
**Pricing.** ¥5–15k per participant per month, employer-paid.
**Risk.** Sensor cost is a hard COGS floor and Abbott/Dexcom control it.
**Call.** `BUILD` · Pull 4 · Moat 3 · TTR 4 — the Japanese occupational health system is the distribution advantage.

#### 130 · Tokutei — specific health checkup follow-up automation
**Pitch.** Close the loop on Japan's mandatory metabolic screening: identify who screened positive, deliver the required guidance, track completion, and report the participation rate the insurer is measured on.
**ICP.** Health insurance societies and large Japanese employers.
**Market.** `A7` Japan digital health $29.2B (2024) → $55.8B (2033) @7.5%; `A39` Medical DX.
**Wedge.** This is a *statutorily mandated programme with a measured participation rate that determines the insurer's financial penalty*. There is no comparable structure in the US or EU, and consequently no foreign competitor. Guidance delivery is still largely manual and paper-based.
**Rivals.** Domestic health-management vendors (Wellness Communications, iCARE), consultancies.
**Pricing.** ¥500–3,000 per covered employee per year.
**Risk.** Health insurance societies are conservative, price-sensitive buyers with long cycles.
**Call.** `BUILD` · Pull 5 · Moat 4 · TTR 4 — mandated, measured, penalized, and unserved by software. Among the best Japan-specific concepts here.

#### 131 · Stress Check — occupational mental health under the Japanese mandate
**Pitch.** Administer Japan's mandatory annual stress check, analyze results at the organizational level, and drive the follow-up interview workflow — while keeping individual results legally separated from the employer.
**ICP.** Japanese employers with 50+ employees (where the check is mandatory).
**Market.** `A16` mental health DTx $4.51B (2026) → $24.42B (2035) @20.64%; `A7` Japan digital health. `A24` APPI — individual stress-check results are sensitive personal information with strict handling rules.
**Wedge.** The legal separation between what the employer may see (aggregate) and what it may not (individual) is a hard privacy-engineering requirement that most HR tools handle badly. Getting it right is both the compliance value and the trust value.
**Rivals.** Domestic providers (Advantage Risk Management, iCARE, Docomo Health).
**Pricing.** ¥300–1,500 per employee per year.
**Risk.** A commoditized checkbox market with heavy price competition; differentiation must come from the follow-up, not the survey.
**Call.** `WEDGE` · Pull 4 · Moat 2 · TTR 5 — mandated demand, commodity delivery.

#### 132 · Kokoro — reimbursed mental health digital therapeutic (Japan)
**Pitch.** A prescription digital therapeutic for depression or anxiety, developed for Japanese reimbursement.
**ICP.** Japanese psychiatrists and their patients, via insurance.
**Market.** `A16` — mental health DTx $4.51B (2026) → $24.42B (2035) @20.64%; **CMS began reimbursing FDA-authorized digital mental health treatments on 2025-01-01**, establishing the payer precedent. `A43` — Japan has approved and reimbursed ~100 AI SaMDs, and CureApp has already proven the Japanese DTx reimbursement path with smoking cessation and hypertension.
**Wedge.** CureApp demonstrated that Japan will reimburse a DTx. The pathway exists and is under-exploited.
**Rivals.** CureApp (domestic leader), Otsuka/Click (whose US joint venture unwound — a cautionary tale), Big Health.
**Pricing.** Reimbursed per prescription.
**Risk.** DTx has a long history of clinical success followed by commercial failure. Pear Therapeutics went bankrupt with FDA clearances in hand. Reimbursement is necessary and demonstrably not sufficient.
**Call.** `WATCH` · Pull 3 · Moat 4 · TTR 1 — the graveyard in this category is large and recent.

#### 133 · EAP+ — employee assistance with outcome measurement
**Pitch.** Employee mental health support that reports clinical outcomes and utilization, not just a call-centre volume figure.
**ICP.** Enterprise HR and benefits.
**Market.** `A16` mental health DTx; US digital mental health $8.97B (2026) @20.25%.
**Wedge.** Traditional EAP utilization runs low single digits and is essentially unmeasured. Measurement is the differentiator — and also the thing that reveals the product does not work, which is why incumbents avoid it.
**Rivals.** Lyra, Spring Health, Modern Health, Headspace.
**Pricing.** $3–15 PEPM.
**Risk.** Extremely crowded in the US; Japan is earlier but far smaller.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 4 — saturated where the money is.

#### 134 · Apnea — sleep apnea screening at scale
**Pitch.** Screen for obstructive sleep apnea using consumer wearable data plus a validated questionnaire, and route positives to diagnosis.
**ICP.** Employers, insurers, primary care.
**Market.** `A13` remote patient monitoring $30.9–67.3B (2026); `A40` CGM as the adjacent metabolic budget.
**Wedge.** Undiagnosed OSA is enormous and drives downstream cardiovascular cost — a clean payer argument.
**Rivals.** Apple and Samsung now ship sleep apnea detection on the watch, which is an existential problem.
**Pricing.** Per-screen $5–20.
**Risk.** Apple shipping this as a free watch feature substantially removes the business.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 4 — the platform vendors already gave it away.

#### 135 · Menopause — workplace menopause support
**Pitch.** Clinical support, symptom tracking and manager guidance for menopause as a workplace retention issue.
**ICP.** Large employers concerned with senior female retention.
**Market.** `A29` femtech $9.78B (2026) → $18.98B (2031) @14.2% — but note the anchor spread runs to **$55.88B, a 5.7× disagreement**; treat femtech sizing with particular suspicion.
**Wedge.** Retention of experienced senior staff is a CFO-legible argument, unlike most femtech framing.
**Rivals.** Peppy, Maven, Elektra Health, Midi.
**Pricing.** $2–8 PEPM.
**Risk.** Benefits buyers are consolidating vendors aggressively; point solutions are being cut.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 4 — right argument, wrong moment in the benefits cycle.

#### 136 · Ninkatsu — fertility benefit navigation for Japan
**Pitch.** Navigate Japan's fertility treatment landscape — now partially insurance-covered — including clinic selection, cycle tracking and employer benefit coordination.
**ICP.** Japanese employers; couples.
**Market.** `A29` femtech $9.78B (2026) @14.2%; `A7` Japan digital health.
**Wedge.** Japan brought fertility treatment into insurance coverage in 2022, and has the demographic urgency and the government attention to keep extending support. Navigation of a newly-covered, complex benefit is a real gap.
**Rivals.** Domestic apps (Luna Luna, ninpath), Carrot and Maven (US, absent from Japan).
**Pricing.** ¥1–5k per employee per month, employer-paid.
**Risk.** Deeply personal data under `A24` APPI sensitivity rules; a breach here is unrecoverable.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — policy tailwind plus demographic urgency.

#### 137 · Boshi — maternal health monitoring linked to the maternal-child record
**Pitch.** Remote monitoring in pregnancy — blood pressure, weight, glucose — integrated with Japan's maternal and child health handbook (母子健康手帳) as it digitizes.
**ICP.** Obstetric clinics, municipalities.
**Market.** `A39` — Medical DX explicitly includes **digitization of maternal-child health services**. `A13` RPM; `A29` femtech.
**Wedge.** The 母子手帳 is a universal, government-run, currently-digitizing artefact. Being the software layer on a national record as it converts is a rare position.
**Rivals.** Municipal app vendors, Baby Kumon-style consumer apps, domestic SIers.
**Pricing.** Municipal contracts ¥5–50M; clinic subscriptions.
**Risk.** Municipal procurement is fragmented across 1,700+ local governments — a brutal go-to-market.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 2 — excellent position, punishing distribution.

#### 138 · Hattatsu — developmental screening for children
**Pitch.** Structured developmental screening at statutory checkup ages, with referral routing and longitudinal tracking.
**ICP.** Municipalities, paediatric clinics, nurseries.
**Market.** `A39` Medical DX maternal-child services; `A7` Japan digital health.
**Wedge.** Japan's statutory infant checkups (1歳半健診, 3歳児健診) are universal touchpoints where developmental concerns are recorded on paper and rarely followed longitudinally.
**Rivals.** Municipal systems, paper.
**Pricing.** Municipal contracts.
**Risk.** Same 1,700-municipality distribution problem as #137, plus the ethical weight of early developmental labelling.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 2 — important work, difficult business.

#### 139 · Junkai — home visit route and capacity optimization
**Pitch.** Optimize home-visit nursing and care routes against traffic, visit duration variance, caregiver qualification and statutory visit requirements.
**ICP.** Japanese home-care and visiting-nursing providers.
**Market.** `A20` — Japan elderly care tech $1.18B (2025) → $3.76B (2033) @15.59%; the **250,000 caregiver shortfall in FY2026** makes each caregiver-hour scarce.
**Wedge.** In a labour shortage, travel time is the largest recoverable inefficiency in home care, and Japanese urban geography makes routing genuinely hard.
**Rivals.** Domestic care software, generic field-service tools.
**Pricing.** ¥30–150k per office per month.
**Risk.** Small operators dominate the sector and have almost no software budget.
**Call.** `WEDGE` · Pull 4 · Moat 2 · TTR 4 — clear ROI, fragmented and poor buyer base.

#### 140 · Zaiko — pharmacy shortage prediction and substitution
**Pitch.** Predict drug shortages from supply signals and manage substitution across a pharmacy network.
**ICP.** Pharmacy chains, hospital pharmacies.
**Market.** `A46` supply chain risk $3.73B (2026) @7.8%; `A3` digital health.
**Wedge.** Japan has had persistent generic drug supply disruption following manufacturing compliance failures, making this locally acute rather than theoretical.
**Rivals.** Wholesaler systems (Medipal, Alfresa, Suzuken), domestic pharmacy software.
**Pricing.** ¥20–100k per pharmacy per month.
**Risk.** The wholesalers hold the supply data and the customer relationship simultaneously.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 4 — the data holder is also the competitor.

#### 141 · Genzai — polypharmacy review and deprescribing support
**Pitch.** Flag patients on dangerous medication combinations and generate a deprescribing proposal for the prescriber, weighted to the elderly.
**ICP.** Japanese hospitals, pharmacies, care facilities.
**Market.** `A20` Japan elderly care; `A39` Medical DX — the national platform will carry **electronic prescriptions**, making cross-provider medication visibility newly possible.
**Wedge.** Japan has the world's oldest population and a fragmented prescribing landscape where no single clinician sees the full medication list. `A39`'s e-prescription platform is what makes solving it feasible for the first time. Timing is the opportunity.
**Rivals.** Domestic pharmacy systems, EMR alert modules.
**Pricing.** ¥30–150k per facility per month.
**Risk.** Depends entirely on e-prescription rollout pace, which is government-controlled and historically slower than announced.
**Call.** `BUILD` · Pull 4 · Moat 4 · TTR 3 — the infrastructure to make this work is being built by the state right now.

#### 142 · Fukuyaku — medication adherence for elderly patients
**Pitch.** Adherence support combining smart dispensing, family visibility and pharmacist escalation.
**ICP.** Families of elderly patients; pharmacies; care providers.
**Market.** `A20` elderly care tech; `A13` RPM.
**Wedge.** Modest — the hardware exists, the behaviour change is hard, and the payer is unclear.
**Rivals.** Hero Health, MedMinder, Japanese pillbox vendors.
**Pricing.** ¥3–8k per patient per month.
**Risk.** Consumer-paid elderly health hardware has a long record of failure; the person who benefits is not the person who buys or operates it.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 4 — nobody owns the outcome.

#### 143 · Byoushou — operating theatre and bed scheduling optimization
**Pitch.** Optimize OR block allocation and bed flow against actual case duration distributions rather than the surgeon's optimistic estimate.
**ICP.** Hospital operations.
**Market.** `A3` digital health; `A33` RCM as the adjacent financial buyer.
**Wedge.** OR time is the highest-value hour in a hospital and is scheduled with remarkably poor statistics.
**Rivals.** LeanTaaS (strong in the US), Qventus, Epic's own tools.
**Pricing.** $150–800k/yr per health system.
**Risk.** Surgeon scheduling is political, not mathematical. The optimization is the easy part.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — the maths works; the politics decide.

#### 144 · Haichi — nurse staffing demand forecasting
**Pitch.** Forecast ward-level nursing demand from admissions, acuity and seasonality, and staff to it rather than to a fixed ratio.
**ICP.** Hospital nursing administration.
**Market.** `A20` caregiver shortage; `A48` — the workforce-shortage logic applies to clinical staffing identically.
**Wedge.** Japanese hospital reimbursement depends on maintaining nurse-to-patient ratios, so mis-staffing has a direct revenue consequence, not just a quality one.
**Rivals.** Domestic HR systems, spreadsheets.
**Pricing.** ¥1–8M per hospital per year.
**Risk.** Nursing unions and rostering customs constrain how much optimization is acceptable.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — the reimbursement link is the argument that closes the deal.

#### 145 · Hashi — SS-MIX2 to FHIR translation for Japan
**Pitch.** Bidirectional translation between Japan's SS-MIX2 standard storage and HL7 FHIR, so Japanese hospital data can reach modern applications.
**ICP.** Japanese hospitals, digital health vendors entering Japan, the Medical DX platform integrators.
**Market.** `A37` — **HL7 FHIR compliance $2.6B (2026) → $8.6B (2036) @12.7%**; healthcare interoperability solutions $5.64B (2026). `A39` — the national platform requires cloud-based data sharing across institutions.
**Wedge.** Every foreign digital health vendor entering Japan hits SS-MIX2 and stops. Every Japanese hospital modernization hits FHIR and stops. One translation layer serves both directions, and `A39` guarantees the traffic.
**Rivals.** Domestic SIers doing it bespoke per project; no productized player.
**Pricing.** ¥5–50M per institution; vendor OEM licensing.
**Risk.** SS-MIX2 implementations vary by site, so "translation" is really 200 slightly different translations.
**Call.** `BUILD` · Pull 5 · Moat 4 · TTR 3 — infrastructure position on a state-mandated migration. Quietly one of the best entries in this document.

#### 146 · MyNa Health — My Number health credential integration
**Pitch.** Let healthcare applications authenticate patients and retrieve consented records through the My Number Card health-insurance credential.
**ICP.** Digital health vendors, clinics, pharmacies.
**Market.** `A39` — **My Number Card as the health insurance credential is an explicit 2026 government priority**, alongside nationwide EMR rollout.
**Wedge.** Being the integration layer for a national identity credential in healthcare is a structurally powerful position, if the government permits third-party intermediation.
**Rivals.** Digital Agency's own reference implementations; the large SIers.
**Pricing.** Per-transaction or vendor licensing.
**Risk.** The state may simply provide this itself and make you redundant overnight. That is a real and reasonably likely outcome.
**Call.** `WATCH` · Pull 5 · Moat 2 · TTR 3 — huge pull, but your competitor is the government and it sets the rules.

#### 147 · Mochidashi — patient data portability
**Pitch.** Let a patient collect their own records from every institution that holds them, in one place, with sharing controls.
**ICP.** Patients; secondarily employers and insurers as sponsors.
**Market.** `A37` healthcare interoperability $5.64B (2026); `A39` Medical DX **data portability** is a named policy objective.
**Wedge.** Policy support is genuine. Consumer willingness to pay is not.
**Rivals.** Apple Health, government PHR apps, domestic vendors.
**Pricing.** Free to consumer; sponsor-funded.
**Risk.** Consumer health record apps have failed repeatedly for twenty years across every market, including with Google and Microsoft behind them.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 3 — the most repeatedly disproven idea in digital health.

#### 148 · Fusei — health insurer fraud and improper claim detection (Japan)
**Pitch.** Detect improper billing patterns across Japanese claims data for insurers and review bodies — phantom visits, unbundling, implausible procedure frequency.
**ICP.** Japanese health insurance societies, review organizations.
**Market.** `A42` — healthcare fraud detection $3.22B (2026) → $7.85B (2031) @19.54%; **claims review is 49.9% of applications**; US FWA is **$308.6B/yr**. Japan-specific fraud figures were **not found in this research** — the market exists by analogy, which is weaker evidence and is scored accordingly.
**Wedge.** Japan's universal claims data is comprehensive and centrally reviewed, making pattern detection unusually tractable.
**Rivals.** Domestic review body systems; IBM, SAS (named in `A42`) absent from Japan.
**Pricing.** ¥10–100M per insurer per year.
**Risk.** No Japanese market data was found. Do not assume US fraud rates transfer to a system with different incentives.
**Call.** `WEDGE` · Pull 3 · Moat 4 · TTR 3 — good structural fit, unproven local demand. Validate before building.

#### 149 · Shika — dental imaging AI and treatment planning
**Pitch.** Caries and bone loss detection on dental radiographs, with treatment plan drafting and patient-facing explanation.
**ICP.** Dental practices, especially the large numbers of small Japanese clinics.
**Market.** `A17` AI medical imaging $2.16B (2026) @30.7%. Dental-specific sizing was **not found**.
**Wedge.** Japan has an unusually high density of small dental practices, and the patient-explanation artefact drives treatment acceptance — which is what the dentist actually pays for.
**Rivals.** Pearl, Overjet, VideaHealth, Denti.AI.
**Pricing.** ¥30–100k per practice per month.
**Risk.** Selling AI that increases treatment recommendations creates an obvious and uncomfortable incentive alignment problem. Pearl and Overjet are also well ahead.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 4 — watch the incentive design closely if you do this.

#### 150 · Doubutsu — veterinary practice intelligence
**Pitch.** Clinical documentation, imaging support and inventory management for veterinary practices.
**ICP.** Small animal veterinary clinics.
**Market.** No clean anchor found in this research. `A9` ambient scribe is the closest analogue and does not cover veterinary.
**Wedge.** Veterinary medicine has no reimbursement complexity, no HIPAA, and a customer who pays cash — which makes it a genuinely faster market to sell into than human health.
**Rivals.** IDEXX (dominant, owns the practice software), Covetrus, Vetstoria.
**Pricing.** $200–1,000 per practice per month.
**Risk.** IDEXX owns the practice management system, the lab and the relationship. Also, no market anchor was found, which means the sizing here is genuinely unknown rather than merely disputed.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 5 — attractive dynamics, unmeasured market, dominant incumbent.


## Part D — AI infrastructure & governance (151–178)

> `A12` is the weakest anchor in this document and every study here says so.
> AI governance is quoted at **$227–340M (2024–25) growing to $4.83B by 2034**.
> That is a real market with a real growth rate and it is *small today* — an
> order of magnitude below healthcare cybersecurity. The pull statistics are
> spectacular (**54% of IT leaders call it a core concern, up from 29%; 40% of
> enterprise apps will embed agents by end-2026; only 6% have advanced AI
> security strategies**) and the spend has not arrived. Concern is not a budget
> line. Price and staff accordingly.

#### 151 · Evidence — evaluation infrastructure for regulated AI
**Pitch.** Held-out, versioned, auditable evaluation suites for AI systems in regulated use, where "we ran evals" must survive an inspector.
**ICP.** Financial services, healthcare, government AI teams.
**Market.** `A12` AI governance $227–340M → $4.83B (2034) @35–45%; `A43` PMDA's 2026-06-05 SaMD guidance now demands AI/ML validation evidence.
**Wedge.** Regulated evaluation is a *records* problem as much as a metrics problem. The eval frameworks are open source and free; the auditable record of which model version was tested against which frozen set on which date is not.
**Rivals.** Braintrust, Langfuse, Arize, Weights & Biases, promptfoo (free).
**Pricing.** $50–300k/yr.
**Risk.** The open-source eval tooling is genuinely excellent and improving fast.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — sell the record, not the metric.

#### 152 · Cite — retrieval attribution verification
**Pitch.** Verify that every claim in a generated answer is actually supported by the retrieved source it cites, and flag the ones that are not.
**ICP.** Anyone shipping RAG into a context where a wrong answer has consequences.
**Market.** `A12` AI governance.
**Wedge.** This is the exhaustiveness rule applied to generation: state what was retrieved, what was used, and what was left unread. A RAG answer that silently drops half its retrieved context is the same failure mode this repository's `docs/SOURCE_EXHAUSTIVENESS.md` exists to prevent.
**Rivals.** Galileo, Patronus, Ragas (open source), Vectara.
**Pricing.** $30–200k/yr.
**Risk.** Increasingly a built-in feature of the model APIs themselves.
**Call.** `WEDGE` · Pull 4 · Moat 2 · TTR 5 — good framing, shrinking space.

#### 153 · Ground — enterprise hallucination monitoring in production
**Pitch.** Sample and score live production LLM output for unsupported claims, with escalation when the rate moves.
**ICP.** Enterprises with customer-facing AI.
**Market.** `A12`.
**Wedge.** Pre-deployment testing does not catch distribution shift in real traffic.
**Rivals.** Galileo, Arize, Fiddler, Patronus.
**Pricing.** $40–250k/yr.
**Risk.** Crowded, undifferentiated, and heading toward being an observability checkbox.
**Call.** `WATCH` · Pull 4 · Moat 1 · TTR 5 — five vendors saying identical things.

#### 154 · Router — inference cost governance and model routing
**Pitch.** Route each request to the cheapest model that meets a measured quality bar, with per-team cost attribution.
**ICP.** Companies whose inference bill has become a board topic.
**Market.** No clean anchor; sits inside cloud cost management.
**Wedge.** Thin and eroding — the model providers keep cutting prices and adding routing themselves.
**Rivals.** OpenRouter, Martian, Portkey, LiteLLM (free), the providers' own routing.
**Pricing.** Percentage of savings, or $20–100k/yr.
**Risk.** Your value proposition shrinks every time a provider cuts prices, which is continuously.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 5 — a business model that erodes on someone else's schedule.

#### 155 · Distil — task-specific small model production
**Pitch.** Turn a customer's frontier-model usage logs into a small fine-tuned model that matches quality on their narrow task at a fraction of the cost and can run on-premises.
**ICP.** High-volume, narrow-task users; regulated firms wanting local inference.
**Market.** `A12`; the on-premises requirement links to `A24` APPI residency and the data-locality demands behind #059.
**Wedge.** Distillation is well understood but operationally fiddly; the recurring value is re-distillation as the task drifts.
**Rivals.** Together, Fireworks, Predibase, in-house ML teams.
**Pricing.** $100–500k/yr.
**Risk.** Frontier model prices are falling fast enough to erase the savings argument for many workloads.
**Call.** `WATCH` · Pull 3 · Moat 2 · TTR 3 — the arbitrage narrows every quarter.

#### 156 · Kokunai AI — sovereign inference for Japanese regulated data
**Pitch.** In-country, audited LLM inference with a contractual guarantee that no data leaves Japan and none is used for training.
**ICP.** Japanese financial institutions, healthcare, government contractors.
**Market.** `A4` Japan cybersecurity $11.36B (2026) → $18.76B (2031) @10.57%; `A24` APPI cross-border rules with **administrative fines expected by 2028**; `A39` Medical DX secondary-use data.
**Wedge.** Japanese regulated buyers have a documented reluctance to send data offshore that predates AI entirely. This is a procurement blocker with a technical solution, and the fines in `A24` are about to make it urgent.
**Rivals.** SoftBank, NTT (tsuzumi), Sakana AI, Fujitsu, plus the hyperscalers' Japan regions.
**Pricing.** Consumption plus a compliance premium.
**Risk.** Capital-intensive, and NTT and SoftBank are building exactly this with balance sheets you cannot match.
**Call.** `WATCH` · Pull 5 · Moat 2 · TTR 3 — correct thesis, wrong company size. This is a telco's business.

#### 157 · Hyouka — Japanese-language model evaluation
**Pitch.** Benchmark and evaluate models on Japanese-specific capability: keigo register, legal and medical terminology, vertical text, and the failure modes English benchmarks miss entirely.
**ICP.** Japanese enterprises selecting models; model developers targeting Japan.
**Market.** `A12` AI governance.
**Wedge.** English benchmarks systematically mislead Japanese buyers, and the register errors (using casual form with a customer) are commercially fatal in ways accuracy metrics do not capture.
**Rivals.** Stability AI Japan, LLM-jp (academic, free), Rakuten AI.
**Pricing.** $30–150k/yr.
**Risk.** Academic consortia publish this for free and are well organized in Japan.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 4 — narrow, real, and probably a services business.

#### 158 · Synth — synthetic data for regulated model training
**Pitch.** Generate statistically faithful synthetic datasets from regulated source data, with a privacy guarantee and a utility measurement.
**ICP.** Healthcare, financial services, anyone blocked from training on real data.
**Market.** `A39` Medical DX secondary use; `A24` APPI; `A47` genomics.
**Wedge.** The utility/privacy trade-off is quantifiable and almost nobody quantifies it honestly for the buyer.
**Rivals.** Gretel (acquired by NVIDIA), MOSTLY AI, Syntegra, Tonic.
**Pricing.** $80–400k/yr.
**Risk.** Regulators are increasingly sceptical that synthetic data escapes privacy obligations at all, which could remove the premise.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 3 — a regulatory ruling could end this category.

#### 159 · Chain — training data provenance and licensing
**Pitch.** Track the licence, source and consent status of every dataset in a training corpus, and flag what cannot lawfully be used.
**ICP.** Model developers, enterprises fine-tuning on mixed corpora.
**Market.** `A12` AI governance; `A18` SBOM as the structural precedent.
**Wedge.** Copyright litigation is turning "where did this data come from" into a discoverable question with financial consequences.
**Rivals.** Fairly AI, Credo AI, in-house legal review.
**Pricing.** $50–250k/yr.
**Risk.** Buyers may prefer not to know, and there is a genuine commercial incentive to remain ignorant.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 4 — litigation, not regulation, will decide the timing.

#### 160 · Broker — least-privilege permission broker for agents
**Pitch.** Sit between an agent and every tool it can call: scope permissions per task, require approval for irreversible actions, log everything, revoke instantly.
**ICP.** Enterprises deploying agents against real systems.
**Market.** `A12` — **40% of enterprise applications will embed autonomous agents by end-2026, and only 6% of organizations have advanced AI security strategies.** `A31` ITDR $3.42B (2026) → $10.51B (2031) @25.17%, where **non-human credentials already cause 41% of identity breaches**.
**Wedge.** Agents are the fastest-growing category of non-human identity and the one with the least governance. Combining `A12`'s adoption curve with `A31`'s breach statistic gives you a problem that is arriving on a known schedule.
**Rivals.** Astrix, Entro, Okta and CyberArk (both moving in), the agent frameworks' own permission models.
**Pricing.** $60–400k/yr.
**Risk.** The agent platforms will build basic permissioning natively; you must be materially better than adequate.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — the two strongest AI statistics in this research point at the same gap.

#### 161 · Recall — agent memory governance
**Pitch.** Govern what agents retain across sessions: retention limits, purpose scoping, subject deletion, and the ability to prove a memory was erased.
**ICP.** Regulated enterprises running persistent agents.
**Market.** `A12` AI governance; `A24` APPI purpose limitation and deletion rights.
**Wedge.** Persistent agent memory collides directly with data-subject deletion rights, and almost nobody has noticed yet.
**Rivals.** None focused; partially handled by vector database vendors.
**Pricing.** $40–200k/yr.
**Risk.** Very early — you would be selling a problem before the buyer has felt it.
**Call.** `WEDGE` · Pull 2 · Moat 4 · TTR 3 — genuinely novel, genuinely premature.

#### 162 · Trace Agent — multi-agent execution observability
**Pitch.** Trace, replay and debug multi-agent workflows across handoffs, with cost and latency attribution per step.
**ICP.** Engineering teams running agent systems in production.
**Market.** `A12`.
**Wedge.** Little — this is the most contested space in AI tooling right now.
**Rivals.** LangSmith, Langfuse, Braintrust, Arize, Datadog.
**Pricing.** $20–150k/yr.
**Risk.** Datadog will bundle it and the open-source options are free and good.
**Call.** `AVOID` · Pull 4 · Moat 1 · TTR 5 — maximum crowding, minimum differentiation.

#### 163 · Report AI — AI incident reporting and register
**Pitch.** Capture, classify and report AI incidents against emerging obligations, with a maintained mapping of what must be reported where and by when.
**ICP.** Enterprises deploying AI in the EU; regulated sectors globally.
**Market.** `A12` AI governance; structurally identical to `A11` and `A24` incident-reporting duties in the cyber domain, which is the precedent worth studying.
**Wedge.** The obligation-matrix-as-moat argument from #063 applies here, one regulatory generation later.
**Rivals.** Credo AI, Holistic AI, OneTrust, IBM watsonx.governance.
**Pricing.** $50–300k/yr.
**Risk.** EU AI Act enforcement timing keeps moving, and buyers wait for enforcement, not for law.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 4 — right structure, uncertain clock.

#### 164 · Conform — EU AI Act conformity assessment workflow
**Pitch.** Classify AI systems by risk tier, assemble the technical documentation, and manage the conformity assessment.
**ICP.** Companies placing high-risk AI systems on the EU market.
**Market.** `A12`.
**Wedge.** Real regulatory demand, but the consultancies are already there and this is judgement-heavy work.
**Rivals.** Credo AI, Holistic AI, Big Four consultancies, TÜV/notified bodies.
**Pricing.** $80–400k/yr.
**Risk.** Notified bodies will own the assessment itself; you get the paperwork around it.
**Call.** `WATCH` · Pull 4 · Moat 2 · TTR 4 — the value accrues to the certifier, not the tool.

#### 165 · Card — model documentation automation
**Pitch.** Generate and maintain model cards, data sheets and technical documentation from the pipeline itself, so documentation cannot drift from reality.
**ICP.** ML platform teams under documentation obligations.
**Market.** `A12`.
**Wedge.** Documentation generated from the artefact rather than written about it is meaningfully better, but it is a feature.
**Rivals.** Weights & Biases, MLflow, Credo AI.
**Pricing.** $20–100k/yr.
**Risk.** A module of the ML platform you already run.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 5 — a feature, priced like one.

#### 166 · Fair — bias auditing for employment and credit AI
**Pitch.** Statutory bias auditing for automated employment and lending decisions, with the published summary those laws require.
**ICP.** Employers and lenders using algorithmic decisioning.
**Market.** `A12` AI governance.
**Wedge.** NYC Local Law 144 and its successors mandate an *independent* audit — and independence is the product. You cannot audit and also sell the remediation, which caps the business but creates the position.
**Rivals.** Holistic AI, BABL AI, Warden AI, employment law firms.
**Pricing.** $20–100k per audit.
**Risk.** Independence requirements structurally prevent the land-and-expand motion that makes SaaS attractive.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — mandated, but the mandate also caps you.

#### 167 · Intake — AI vendor risk assessment for procurement
**Pitch.** Assess the AI capabilities and risks of software a company is *buying* — what model, what data flows, what retention, what indemnity.
**ICP.** Enterprise procurement and vendor risk teams.
**Market.** `A8` TPRM $10.60B (2026) → $20.71B (2031) @14.34%; `A12` AI governance.
**Wedge.** Every SaaS vendor added AI features in 2024–25 and almost no customer re-assessed them. The re-assessment backlog is enormous, real, and currently handled with a spreadsheet.
**Rivals.** Vanta, Panorays, SecurityScorecard, OneTrust (all adding AI questionnaires).
**Pricing.** $30–200k/yr.
**Risk.** A questionnaire module inside TPRM platforms that already have the customer.
**Call.** `WEDGE` · Pull 4 · Moat 2 · TTR 5 — real backlog, weak defensibility.

#### 168 · Terms — AI clause review in commercial contracts
**Pitch.** Extract and flag AI-relevant clauses — training rights, output ownership, indemnity, model change notification — across a contract portfolio.
**ICP.** Legal operations at large enterprises.
**Market.** `A12`; legal tech had no clean anchor here.
**Wedge.** Modest. The contract AI vendors are already doing this.
**Rivals.** Ironclad, Luminance, Harvey, Robin AI.
**Pricing.** $50–250k/yr.
**Risk.** A prompt inside an incumbent product.
**Call.** `AVOID` · Pull 3 · Moat 1 · TTR 5 — a feature of a market that is already consolidating.

#### 169 · Poison — training data integrity verification
**Pitch.** Detect poisoned or manipulated examples in fine-tuning datasets before they reach the model.
**ICP.** Model developers; enterprises fine-tuning on third-party or user-generated data.
**Market.** `A12`; `A18` supply chain security as the structural analogue.
**Wedge.** The threat is well documented in research and essentially unaddressed in production tooling.
**Rivals.** HiddenLayer, Protect AI (Palo Alto), academic tooling.
**Pricing.** $50–250k/yr.
**Risk.** No buyer has yet been visibly burned in public, and security budgets follow incidents, not papers.
**Call.** `WATCH` · Pull 2 · Moat 4 · TTR 3 — waiting for the incident that creates the market.

#### 170 · Origin — content provenance at capture
**Pitch.** C2PA-conformant signing of images and video at the point of capture, with verification infrastructure for publishers.
**ICP.** News organizations, insurers, courts, camera manufacturers.
**Market.** `A14` — **8M deepfakes circulating in 2026 versus 500,000 in 2023**; detection quoted at ~$15.7B by 2026 at ~42%, though that figure looks aggressive relative to its peers.
**Wedge.** Proving what is real scales; detecting what is fake does not. The detection arms race is unwinnable in principle, and provenance sidesteps it. This is the strategically correct side of the deepfake problem.
**Rivals.** Truepic, C2PA coalition (Adobe, Microsoft, Sony, Nikon, Leica — all shipping it in hardware).
**Pricing.** SDK licensing; verification-as-a-service.
**Risk.** The camera manufacturers and Adobe are implementing the standard directly. Your position is at best complementary to theirs.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — right side of the problem, powerful incumbents already on it.

#### 171 · Detector — AI-generated text detection for education
**Pitch.** Detect AI-generated student work.
**ICP.** Schools and universities.
**Market.** `A14` synthetic media.
**Wedge.** None. The detection does not work reliably, the false-positive rate falls hardest on non-native English writers, and students have been wrongly accused on the strength of these tools.
**Rivals.** Turnitin, GPTZero, Originality.
**Pricing.** Per-seat institutional.
**Risk.** Selling a tool with a known, documented, discriminatory false-positive rate into a setting where the consequence is academic expulsion. The product does not do what it claims.
**Call.** `AVOID` · Pull 4 · Moat 1 · TTR 5 — high demand for something that does not work and harms people when it fails. Listed because refusing it is part of an honest survey.

#### 172 · Liveness — voice anti-spoofing for contact centres
**Pitch.** Detect synthetic and replayed voice in real time during a live call, before the agent authenticates the caller.
**ICP.** Banks, insurers, contact centre operators.
**Market.** `A14` — **voice cloning $4.06B (2026) @23.9%; as little as 3 seconds of audio yields an 85%-accurate clone; vishing attacks up 1,633% in Q1 2025; one in ten Americans has already experienced a voice-clone scam.** `A44` BEC losses $3.05B in 2025.
**Wedge.** Voice biometrics were sold to contact centres for a decade as an authentication factor and are now an attack surface. Every deployment is a liability that must be retrofitted — that is a rip-and-replace cycle with a known installed base.
**Rivals.** Pindrop, Nuance Gatekeeper, Reality Defender, ID R&D.
**Pricing.** Per-call $0.01–0.10; enterprise floors.
**Risk.** Pindrop is strong and has the contact-centre relationships.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 3 — the 1,633% figure is the most violent growth statistic in this research.

#### 173 · Forge — document forgery detection
**Pitch.** Detect manipulated PDFs, invoices, bank statements and identity documents at the pixel and metadata level.
**ICP.** Lenders, insurers, KYC operations, accounts payable.
**Market.** `A14` deepfake and synthetic media; `A19` AML/KYC; `A44` — **85 cents of every cybercrime dollar lost goes to cyber-enabled fraud**.
**Wedge.** Generative tools made convincing document forgery trivial and free in the space of two years. Every lending and onboarding process still assumes documents are hard to fake.
**Rivals.** Resistant AI, Inscribe, Ocrolus, Persona.
**Pricing.** Per-document $0.10–2.
**Risk.** Resistant AI and Inscribe are well established in lending specifically.
**Call.** `WEDGE` · Pull 5 · Moat 3 · TTR 4 — strong pull, credible incumbents, room in adjacent verticals.

#### 174 · Onboard — deepfake-resistant identity verification
**Pitch.** Remote identity verification hardened against injection attacks and synthetic presentation — not just liveness, but detection of a virtual camera feeding a generated face.
**ICP.** Financial services, crypto exchanges, gig platforms.
**Market.** `A14` — **deepfakes now drive 1 in 5 biometric fraud attempts**.
**Wedge.** Injection attacks bypass presentation-attack detection entirely, and much of the installed IDV base is only tested against presentation attacks.
**Rivals.** iProov (strong here), Jumio, Onfido (Entrust), Persona, Incode.
**Pricing.** Per-verification $0.50–3.
**Risk.** iProov has been solving exactly this for years and has the certifications.
**Call.** `WATCH` · Pull 5 · Moat 2 · TTR 4 — huge pull, a specialist already owns the technical high ground.

#### 175 · Composite — synthetic identity detection
**Pitch.** Detect fabricated identities assembled from real fragments — the fraud type that passes every document check because every element is genuine.
**ICP.** Banks, lenders, telcos.
**Market.** `A19` AML $3.84B (2025) → $10.74B (2035); `A44` cyber-enabled fraud dominance.
**Wedge.** Synthetic identity is invisible to document verification by construction; it is only detectable as a pattern across institutions, which means consortium data.
**Rivals.** SentiLink (strong), Socure, LexisNexis.
**Pricing.** Per-application $0.20–2.
**Risk.** Consortium data is the moat and you start with none of it, which is close to a cold-start impossibility in the US.
**Call.** `WATCH` · Pull 4 · Moat 4 · TTR 2 — right problem, and the moat belongs to whoever got there first.

#### 176 · Tegaki — handwritten Japanese form processing
**Pitch.** Accurate extraction from handwritten Japanese forms — the administrative substrate of Japanese government, healthcare and banking.
**ICP.** Municipalities, hospitals, banks, insurers.
**Market.** `A39` Medical DX digitization; `A7` Japan digital health; `A20` care documentation.
**Wedge.** Handwritten Japanese OCR — kanji, mixed scripts, vertical text, ruled forms, individual handwriting variance — is materially harder than Latin-script OCR, and the global document-AI vendors treat it as an afterthought. Japan's paper burden is unusually large, which makes the payoff unusually large.
**Rivals.** Cogent Labs (domestic, focused on exactly this), AI inside, Fujitsu, Google Document AI (weak on handwritten Japanese).
**Pricing.** Per-page ¥5–50; enterprise licensing.
**Risk.** Cogent Labs and AI inside have a genuine head start and domestic distribution.
**Call.** `WEDGE` · Pull 5 · Moat 4 · TTR 4 — a hard technical problem protecting a large, boring, well-funded market.

#### 177 · Houmu — Japanese contract review
**Pitch.** Contract review tuned to Japanese legal drafting conventions and the specific risk clauses Japanese in-house counsel check for.
**ICP.** Japanese corporate legal departments.
**Market.** No clean anchor found. `A12` AI governance is adjacent but not the buyer.
**Wedge.** Japanese contract drafting conventions differ enough that English-trained review tools mislead. But the domestic incumbents already understand this.
**Rivals.** LegalForce (MNTSQ, LegalOn — well funded and domestic), Hubble.
**Pricing.** ¥100k–1M per month.
**Risk.** LegalOn is strong, well capitalized and already the category answer in Japan.
**Call.** `AVOID` · Pull 4 · Moat 2 · TTR 4 — the domestic incumbent already won this.

#### 178 · Watch — multi-jurisdiction regulatory change monitoring
**Pitch.** Track regulatory change across the jurisdictions a company operates in, map each change to the internal controls it affects, and open the remediation task.
**ICP.** Compliance functions in multinationals.
**Market.** `A27` eGRC $57.10B (2026) @10.8%, GRC platforms $23.32B (2026). The regulatory catalysts in this document alone — `A11` ACD Law (2026-10-01), `A24` APPI amendment (April 2026, fines by 2028), `A43` PMDA SaMD guidance (2026-06-05) — illustrate the volume a single company must track in one country.
**Wedge.** Mapping change to *controls* rather than delivering a newsletter. The alerting is commoditized; the control linkage is not.
**Rivals.** Thomson Reuters, Wolters Kluwer, Ascent, Corlytics.
**Pricing.** $80–500k/yr.
**Risk.** The legal publishers own the content and the customer, and they have owned both for a century.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 3 — win on the control mapping or don't play.

---

## Part E — Crossovers: health × cyber × OSINT (179–200)

> The most defensible concepts in this document are in this section, for a
> structural reason: a crossover product needs two competences that rarely sit
> in one company, which is exactly what keeps well-funded single-domain
> incumbents out.

#### 179 · Kanja — patient data exposure monitoring
**Pitch.** Monitor breach dumps, ransomware leak sites and dark web markets specifically for a health system's patient data, and drive the notification obligation from evidence rather than assumption.
**ICP.** Hospital privacy officers and CISOs.
**Market.** `A10` dark web monitoring $1.2B (2025) → $4.1B (2034) @14.6%; `A45` — **770 HIPAA breaches in 2025, a record; Change Healthcare exposed 192.7M individuals; Q1 2026 affected-individual counts up 29.4% year over year**. `A6` healthcare cybersecurity $42.31B (2026).
**Wedge.** Breach notification obligations turn on *what was actually exposed*. Health systems currently notify on worst-case assumptions because they cannot see the leak site. Evidence narrows the notification population, and notification cost scales with headcount — a direct, calculable saving.
**Rivals.** SpyCloud, Recorded Future, Fortra, Censinet (adjacent).
**Pricing.** $60–300k/yr per health system.
**Risk.** Leak-site access has legal complexity in some jurisdictions, and the data is intermittent by nature.
**Call.** `BUILD` · Pull 5 · Moat 3 · TTR 4 — cost avoidance you can compute on the whiteboard during the first meeting.

#### 180 · Integrity — clinical trial data fraud detection
**Pitch.** Detect fabricated or manipulated data at trial sites — implausible distributions, digit preference, impossible visit timing, duplicated patients across sites.
**ICP.** Sponsors, CROs, regulators.
**Market.** `A22` DCT $10.74–14.29B (2026) @14.42%; clinical trials overall heading to **$176.32B by 2030**. `A42` fraud detection methodology transfers directly.
**Wedge.** Statistical fraud detection is well established in academic literature and almost absent from commercial trial monitoring. Sponsors currently find fabrication at audit, years late, at enormous cost.
**Rivals.** Medidata (risk-based monitoring, partial), CluePoints (closest), IQVIA.
**Pricing.** $150k–1M per programme.
**Risk.** CluePoints does this credibly already, and sponsors are reluctant to go looking for problems in their own pivotal trials.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 3 — strong method, uncomfortable incentive on the buyer's side.

#### 181 · Nisemono — pharmaceutical counterfeit and diversion intelligence
**Pitch.** Monitor marketplaces, social channels and grey-market listings for counterfeit and diverted product, and act on it.
**ICP.** Pharma brand protection; especially GLP-1 and high-value biologics.
**Market.** `A23` DRP $1.20B (2025) → $5.51B (2034) @18.5%; `A46` supply chain risk $3.73B (2026).
**Wedge.** GLP-1 shortages created a large, dangerous counterfeit market almost overnight, and patient harm makes this a regulatory and reputational emergency rather than a revenue-leakage problem.
**Rivals.** MarqVision, Red Points, Corsearch, pharma internal teams.
**Pricing.** $80–400k/yr per brand.
**Risk.** Generic brand-protection vendors compete on price with adequate coverage.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — the patient-harm framing is what justifies the premium over generic takedown vendors.

#### 182 · Iryou — medical misinformation monitoring for health authorities
**Pitch.** Track health misinformation narratives — vaccine claims, unproven treatments, dangerous advice — with early warning as a narrative gains velocity.
**ICP.** Public health agencies, hospital systems, pharma medical affairs.
**Market.** `A34` disinformation operations $500M (2025) → $1.8B (2034) @15%; `A14` synthetic media.
**Wedge.** Health misinformation is measurably lethal, which gives public health agencies a mandate — but public health budgets are small and politically exposed.
**Rivals.** Logically, NewsGuard, academic monitoring groups.
**Pricing.** $80–400k/yr.
**Risk.** Politically charged in a way that makes commercial buyers nervous and government buyers slow.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 3 — real harm, difficult market.

#### 183 · Yakkihou — health claim advertising compliance (Japan)
**Pitch.** Monitor a company's own marketing, its affiliates and its influencers for claims that breach Japan's Pharmaceutical and Medical Device Act advertising restrictions, which are stricter than most markets expect.
**ICP.** Japanese cosmetics, supplement, health food and medical device companies.
**Market.** `A7` Japan digital health; `A43` Japanese regulatory environment; `A23` DRP for the monitoring infrastructure.
**Wedge.** 薬機法 advertising rules are strict, enforced, and routinely breached by affiliate marketers and influencers acting outside the brand's control — while the brand carries the liability. This is a large, anxious, well-funded Japanese buyer with no good tool.
**Rivals.** Domestic legal consultancies doing manual review; no software incumbent.
**Pricing.** ¥300k–3M per month.
**Risk.** Requires genuine legal judgement encoded into classification, and being wrong costs the customer a regulatory finding.
**Call.** `BUILD` · Pull 5 · Moat 4 · TTR 4 — a strictly enforced, jurisdiction-specific rule with liability attached and no vendor serving it.

#### 184 · Sentinel — outbreak signal detection from open sources
**Pitch.** Detect emerging disease signals from local news, pharmacy sales, school absence data, obituaries and clinical chatter, ahead of formal reporting.
**ICP.** Public health agencies, pharma, reinsurers, large employers.
**Market.** `A25` geospatial analytics; `A34` open-source monitoring; `A3` digital health.
**Wedge.** Formal surveillance lags by design because of case confirmation. Open sources lead by days to weeks, which is the difference between response and reaction.
**Rivals.** BlueDot, Metabiota (Ginkgo), HealthMap (academic, free), Airfinity.
**Pricing.** $100–600k/yr.
**Risk.** Pandemic-era funding has receded sharply and the buyers' attention with it. BlueDot's post-2022 trajectory is instructive.
**Call.** `WATCH` · Pull 3 · Moat 4 · TTR 3 — excellent capability, procyclical market. Build it during an outbreak, sell it during the next one.

#### 185 · Dual Use — biosecurity research monitoring
**Pitch.** Monitor published research, preprints and equipment procurement for dual-use biological risk indicators.
**ICP.** Government biosecurity offices, funders, journals.
**Market.** No clean market anchor. Government-funded.
**Wedge.** Genuine national security relevance, essentially no commercial market.
**Rivals.** NTI and academic centres, mostly grant-funded.
**Pricing.** Government contracts.
**Risk.** Single-buyer, classified-adjacent, and the work itself is ethically delicate.
**Call.** `WATCH` · Pull 2 · Moat 4 · TTR 2 — important, not a company.

#### 186 · Kesson — drug shortage early warning from open signals
**Pitch.** Predict drug shortages from manufacturing inspection findings, import records, regulatory actions and API supplier signals — weeks before the shortage notice.
**ICP.** Hospital pharmacies, group purchasing organizations, health ministries.
**Market.** `A46` supply chain risk $3.73B (2026) @7.8%; `A6` healthcare.
**Wedge.** Inspection findings and import anomalies are public and are genuinely leading indicators of supply failure. Japan's ongoing generic supply disruption makes this locally urgent, and the underlying signals are exactly the sort of public-source collection this repository is built for.
**Rivals.** Vizient, Premier (GPO-internal), the regulators' own shortage lists (lagging by definition).
**Pricing.** ¥10–80M per hospital group.
**Risk.** GPOs may consider this their own function and build it internally.
**Call.** `BUILD` · Pull 4 · Moat 4 · TTR 3 — a clean OSINT method applied to a clinical problem with a measurable cost.

#### 187 · Perimeter — health system external exposure monitoring
**Pitch.** Continuous external attack surface monitoring specific to health systems: patient portals, imaging shares, clinic networks acquired in the last merger and never inventoried.
**ICP.** Health system CISOs.
**Market.** `A5` ASM $1.65–2.03B (2026) up to 31.3% CAGR; `A6` healthcare cybersecurity $42.31B (2026); `A45` **99% of hospitals have vulnerable devices; 2.3 ransomware attacks per day in H1 2026**.
**Wedge.** Health systems grow by acquiring practices and inherit their IT estates wholesale, without inventory. The unknown-asset problem is structurally worse in healthcare than anywhere else.
**Rivals.** All of `A5`'s vendor list; Censinet in healthcare specifically.
**Pricing.** $60–300k/yr.
**Risk.** Generic ASM covers most of this competently.
**Call.** `WEDGE` · Pull 5 · Moat 2 · TTR 4 — the acquisition-sprawl framing is the only real differentiator.

#### 188 · Recall — medical device recall and advisory intelligence
**Pitch.** Track device recalls, safety advisories and vulnerability disclosures across every regulator, match them to the customer's actual device inventory, and generate the required action.
**ICP.** Hospital biomedical engineering and risk management.
**Market.** `A6` medical device security $8.30B (2026) → $22.69B (2034) @13.39%; `A36` post-market surveillance.
**Wedge.** Matching a global recall stream to a *local inventory* is where the work is, and where every hospital currently uses email and hope.
**Rivals.** Asimily, Nuvolo, RLDatix (adjacent).
**Pricing.** $40–200k/yr.
**Risk.** Depends on inventory data quality that hospitals often do not have — you may need to solve #098 first.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — pairs naturally with #098 and #100.

#### 189 · Network — provider network and health system intelligence
**Pitch.** Map provider affiliations, referral patterns, ownership changes and network adequacy from public and claims data.
**ICP.** Payers, health system strategy teams, pharma commercial.
**Market.** `A47` precision medicine commercial adjacency; `A33` RCM.
**Wedge.** Modest. Well served in the US.
**Rivals.** Definitive Healthcare, H1, Trilliant Health.
**Pricing.** $100–500k/yr.
**Risk.** Mature US market; Japan's provider landscape is far less commercially interesting because pricing is nationally set.
**Call.** `AVOID` · Pull 3 · Moat 2 · TTR 4 — served in the US, structurally uninteresting in Japan.

#### 190 · Ring — healthcare fraud ring detection by graph
**Pitch.** Find organized fraud rings by graph structure across claims, providers, patients, addresses and corporate ownership — the patterns single-claim scoring can never see.
**ICP.** Payers, government health programmes, review bodies.
**Market.** `A42` — healthcare fraud detection $3.22B (2026) → $7.85B (2031) @19.54%; **$308.6B/yr lost to US fraud, waste and abuse; over 70% of healthcare organizations are adopting AI fraud detection; graph analytics is explicitly named as the technique that identifies rings single-claim analysis misses**; government agencies are the fastest-growing segment at 22.05% CAGR.
**Wedge.** The research names graph analytics as the differentiator, and this is precisely an OSINT technique — entity resolution across corporate ownership, addresses and identities — applied to claims. It is the clearest example in this document of the two skill sets combining.
**Rivals.** SAS, IBM, Optum, Codoxo, Cotiviti.
**Pricing.** $200k–2M per payer; gainshare common.
**Risk.** Incumbents with decades of payer relationships and their own claims data.
**Call.** `BUILD` · Pull 5 · Moat 4 · TTR 3 — the single best method-to-market fit across the whole document.

#### 191 · Site Check — sanctions and integrity screening for trial sites
**Pitch.** Screen clinical trial sites, investigators and vendors against sanctions, debarment lists, research misconduct findings and adverse media before contracting.
**ICP.** Sponsors, CROs.
**Market.** `A22` DCT $10.74–14.29B (2026); `A19` AML/KYC screening infrastructure.
**Wedge.** Sponsors run trials in dozens of countries and screen investigators inconsistently. Debarment and misconduct data is public and rarely integrated.
**Rivals.** Sponsors' internal compliance; generic screening vendors with no trial context.
**Pricing.** $50–250k/yr.
**Risk.** Narrow; likely a module inside a larger clinical vendor.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 4 — small, clean, defensible.

#### 192 · Retract — research integrity and paper mill detection
**Pitch.** Detect fabricated papers, paper-mill output, image duplication and citation rings before publication or funding.
**ICP.** Publishers, funders, universities, pharma literature teams.
**Market.** `A36` literature monitoring $234.73M (2026) PV software as the adjacent commercial buyer; publishing integrity had **no clean anchor**.
**Wedge.** Paper mills are an industrialized problem and publishers are visibly losing to them. Pharma also has a direct interest: `A36` shows drug safety signal detection consumes literature, and poisoned literature poisons the signal.
**Rivals.** Signals from STM Integrity Hub, Proofig, ImageTwin, Clear Skies.
**Pricing.** $50–300k/yr.
**Risk.** Publisher budgets are thin and the incumbents are consortium-owned.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 4 — sell to pharma safety teams, not to publishers.

#### 193 · Postmortem — AI incident intelligence
**Pitch.** A curated, structured database of real AI failures — with the technical cause, the affected system class and the mitigation — so an enterprise can check whether its own deployment shares the failure mode.
**ICP.** AI governance teams, insurers underwriting AI risk, regulators.
**Market.** `A12` AI governance; `A21` cyber insurance $23.29B (2026) as the model for how incident data becomes underwriting data.
**Wedge.** The cyber industry took twenty years to build shared incident intelligence. AI has none, and `A12` says **only 6% of organizations have advanced AI security strategies** — largely because nobody knows what actually goes wrong at scale.
**Rivals.** AI Incident Database (non-profit, free), OECD AI incident work.
**Pricing.** $30–200k/yr.
**Risk.** The free non-profit version exists and is credible; you must be substantially more structured and current.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 4 — the insurance angle is what makes it a business rather than a wiki.

#### 194 · Autonomous — agentic penetration testing
**Pitch.** Continuously running agents that attempt real attack chains against a customer's environment and report reproducible paths, not findings.
**ICP.** Mid-market and enterprise security teams.
**Market.** `A32` PTaaS $0.72B (2026) → $1.98B (2031) @22.6%; `A49` CTEM $1.48B (2026); `A48` — **88% of organizations had a security event attributable to a skills shortage**, and 72% role fill rate.
**Wedge.** The workforce data in `A48` says the skills to do this manually are structurally unavailable. Automation is not a cost saving here, it is the only way the work happens.
**Rivals.** Pentera, Horizon3, XBOW, Mindfort, RunSybil.
**Pricing.** $50–300k/yr.
**Risk.** Extremely hot space with many funded entrants; also, an autonomous agent attacking production is one bug away from an outage you caused.
**Call.** `WEDGE` · Pull 5 · Moat 2 · TTR 4 — right thesis, very crowded, real operational danger.

#### 195 · Converge — physical and cyber security convergence for hospitals
**Pitch.** One view of physical access, badge anomalies, camera events and network events — because infant abduction, drug diversion and data theft are all access-control failures.
**ICP.** Hospital security and compliance.
**Market.** `A6` healthcare cybersecurity $42.31B (2026); `A38` insider risk $4.5–7.09B (2026), average annualized cost **$19.5M**.
**Wedge.** Drug diversion is a specific, prosecutable, measurable hospital problem that sits exactly on the physical/digital boundary and is owned by neither team.
**Rivals.** Invistics (diversion-specific), Imprivata, Genetec.
**Pricing.** $80–400k/yr.
**Risk.** Two buying centres means two budgets and two sets of objections.
**Call.** `WEDGE` · Pull 3 · Moat 3 · TTR 3 — narrow on drug diversion first, expand later.

#### 196 · Netsu — heat-health early warning for Japan
**Pitch.** Facility- and population-level heatstroke risk warning combining meteorological forecast, building conditions and vulnerable-population registers, for municipalities and employers.
**ICP.** Japanese municipalities, construction firms, logistics companies, care facilities.
**Market.** `A20` elderly care (heatstroke mortality in Japan is concentrated in the elderly); `A7` Japan digital health; `A25` geospatial.
**Wedge.** Japan has a national heatstroke alert system, an ageing population disproportionately affected, and employers with occupational safety duties. Linking the public forecast to a specific vulnerable individual or worksite is the unserved step — and it is exactly a public-data-fusion problem.
**Rivals.** JMA public alerts (free, population-level), domestic occupational safety vendors.
**Pricing.** ¥3–30M per municipality or enterprise per year.
**Risk.** The free public alert is "good enough" in most buyers' judgement until someone dies.
**Call.** `WEDGE` · Pull 4 · Moat 3 · TTR 4 — strong Japanese fit, competing against a free baseline.

#### 197 · Jishin — health system continuity for seismic events
**Pitch.** Pre-computed continuity planning for hospitals in a major seismic event: staff reachability, patient transfer capacity, supply positioning, and post-event structural triage from public hazard data.
**ICP.** Japanese hospitals, prefectural health departments, disaster base hospitals.
**Market.** `A25` geospatial imagery analytics $16.19B (2026) @27.7%; `A7` Japan digital health; `A45` for the general logic that clinical continuity is a distinct, underbuilt discipline.
**Wedge.** Japan mandates disaster planning for designated base hospitals, but the plans are documents rather than live models. Public hazard, liquefaction and transport data make a live model genuinely possible — and this repository's collector fleet already carries much of that data class.
**Rivals.** Domestic SIers and consultancies producing planning documents.
**Pricing.** ¥10–100M per prefecture or hospital group.
**Risk.** Procurement is public-sector, slow, and typically awarded to incumbent integrators.
**Call.** `WEDGE` · Pull 4 · Moat 4 · TTR 2 — high national relevance, hard commercial path.

#### 198 · Kaishu — food safety recall and contamination intelligence
**Pitch.** Aggregate global food recalls, inspection findings and outbreak signals, match against a company's actual supplier and ingredient list, and trigger the response.
**ICP.** Food manufacturers, retailers, restaurant chains.
**Market.** `A46` supply chain risk $3.73B (2026) @7.8%; `A25` for the monitoring layer.
**Wedge.** Ingredient-level matching against a live global recall stream is the work; the recalls themselves are public and free.
**Rivals.** FoodChain ID, TraceGains, Hazel Analytics.
**Pricing.** $40–250k/yr.
**Risk.** Served adequately in the US; Japan is thinner but smaller.
**Call.** `WATCH` · Pull 3 · Moat 3 · TTR 4 — solid, unexciting, already served.

#### 199 · Mizu — environmental health exposure monitoring
**Pitch.** Link environmental monitoring data — water quality, air, soil contamination, industrial emissions — to population health outcomes at a local level.
**ICP.** Municipalities, public health researchers, litigation, insurers.
**Market.** `A25` geospatial analytics; `A3` digital health.
**Wedge.** PFAS and similar contamination questions are becoming litigation-relevant, which creates a payer with real money — but the buyer is a plaintiff firm, not a health department.
**Rivals.** Academic and government monitoring (free), environmental consultancies.
**Pricing.** Project-based, $50–500k.
**Risk.** The data is public and free; you are selling analysis, which is consulting.
**Call.** `WATCH` · Pull 2 · Moat 3 · TTR 3 — a consultancy with a database.

#### 200 · Genten — provenance-first intelligence infrastructure
**Pitch.** The engine underneath most of Parts A and E: thousands of public sources fetched on their own schedules into one searchable, geo-aware store, where every record carries its fetch provenance, nothing is silently truncated between collection and display, and a failed fetch degrades to an explicit gap rather than to invented content.
**ICP.** Any of the above verticals; sold as infrastructure or as the basis of a vertical product.
**Market.** `A1` OSINT (spread-flagged, $5.22–22.35B for 2026); `A2` threat intelligence $11.55B (2025) → $22.97B (2030) @14.7%; `A25` geospatial $16.19B (2026) @27.7%. Every vertical in Parts A and E is a downstream application of one engine.
**Wedge.** The pattern across this entire document is that vertical intelligence products are mostly the same collection engine wearing different clothes — #001 vessels, #006 supply chains, #186 drug shortages, #190 fraud rings and #198 food recalls are one system and five schemas. What separates a trustworthy engine from an untrustworthy one is not source count but whether it can prove what it actually fetched. Most cannot: an intelligence platform that quietly returns page one of a paginated source, or substitutes a cached value for a failed fetch, is indistinguishable from a correct one right up until an analyst stakes a decision on it.
**Rivals.** Palantir Foundry (vastly larger, vastly more expensive), Recorded Future, Maltego, and every in-house scraping stack.
**Pricing.** OEM and platform licensing $100–500k/yr; vertical products priced per Parts A–E.
**Risk.** Horizontal infrastructure is famously hard to sell; buyers purchase answers, not engines. The realistic path is to win one vertical first and let the engine follow the product, not lead it.
**Call.** `BUILD` · Pull 3 · Moat 5 · TTR 2 — the lowest pull score and the highest moat score in this document, which is the honest shape of infrastructure. It is also the only entry here whose core claim this codebase has already demonstrated.

---

## 3. What the research actually says

Five conclusions that came out of the data rather than out of the brief.

**1. Dated regulation beats market size, every time.** The highest-conviction
concepts here (#035, #037, #049, #059, #130, #145, #183) are not in the biggest
markets. They sit against an obligation with a date: `A11`'s **2026-10-01**,
`A24`'s fines **by 2028**, `A43`'s **2026-06-05** guidance, `A39`'s EMR
rollout. A CFO argues with a TAM. A CFO does not argue with a statute.

**2. The fraud has moved and the industry has not.** `A44` is the most
consequential single line in this research: of **$20.9B in 2025 cybercrime
losses, nearly 85 cents in every dollar went to cyber-enabled fraud — someone
receiving something convincing and acting on it — not to malware or zero-days.**
Most security spending still defends the other 15 cents. #083, #085, #172 and
#173 follow directly from this and are, collectively, the strongest thesis in
Part B.

**3. Healthcare's cyber problem is now a clinical problem.** `A45` reports
**2.3 ransomware attacks per day on healthcare in H1 2026** and **34–38% higher
in-hospital mortality for patients admitted when an attack begins**. That
converts the category from an IT risk to a patient-safety one, which changes the
buyer, the budget and the urgency. #099 and #074 exist because of that number,
and nobody is selling to that buyer yet.

**4. The gap between AI adoption and AI governance is the whole AI
opportunity.** `A12`: **40% of enterprise applications will embed autonomous
agents by end-2026, and 6% of organizations have advanced AI security
strategies.** But note what `A12` also says — the governance market is only
$227–340M today. The gap is real and the money has not arrived. Build for 2027,
price for 2026.

**5. Japan's advantage is linguistic and regulatory, not technical.** Every
strong Japan concept here (#002, #003, #093, #117, #130, #176, #183) wins for
the same reason: a domain that is genuinely hard to enter from outside — kanji
name resolution, the biennial fee schedule, 薬機法 advertising rules, handwritten
form processing, the 特定健診 mandate. None of them wins on algorithms. If a
Japan play's advantage would survive translation, it is not a Japan play.

### If you build one thing

**#049 (CBOM)** if you want the biggest market — `A41` is the fastest-growing
anchor found (37.8%), with a named, sourced blocker (**61% intend to migrate,
41% are preparing, and poor cryptographic visibility is cited as the reason**).

**#190 (fraud ring graph)** if you want the best fit between method and market —
`A42` explicitly names graph analytics as the technique that works, against
**$308.6B/yr** of loss, and it is an OSINT entity-resolution problem wearing a
healthcare badge.

**#099 (clinical downtime continuity)** if you want the one where being right
matters most. `A45`'s mortality figure is not a market statistic. It is the
reason the product should exist.

### What would change these conclusions

This research is desk research, and the honest failure modes are:

- **No customer contact.** Every "wedge" here is a hypothesis about a buyer
  nobody asked. Ten calls per concept would kill perhaps a third of them.
- **Anchors that disagree by up to 80×** (`A23`). Where a study leans on a
  disputed anchor, it says so, but a 3× error in a SAM changes a build/skip
  decision.
- **Regulatory dates move.** `A11` full effect is 2027, `A24` fines are
  "expected by 2028", and the EU AI Act timetable has slipped before. Several
  Pull scores here are really bets on a calendar.
- **Competitor positions are from search results, not from teardowns.** Where
  this document says a rival is "strong", that is inference from market
  presence, not a product evaluation.

_All market figures retrieved 2026-08-10 and sourced in §1. SAM derivations,
wedges, pricing, risks and verdicts are analysis, not sourced fact._
