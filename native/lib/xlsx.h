/* lib/xlsx.h — one worksheet of an Office Open XML workbook (.xlsx) as
 * RFC 4180 CSV text.
 *
 * WHY THIS EXISTS. Japanese ministries, the FSA, the BOJ and the industry
 * associations publish their registers as an .xlsx sibling next to the PDF —
 * the licensed-operator lists, the 適格機関投資家 roster, the 特定商取引法
 * disposition table. The engine reads json/csv/xml/html; it had no way to
 * take those bytes at all, so the machine-readable copy of a register the
 * tree already scrapes as PDF text was being left on the server. This turns
 * the workbook into CSV so the EXISTING csv row path (header detection,
 * csv_delim/csv_comment, title_keys/id_keys, the UID-collision guard) is
 * reused unchanged: one call at the top of the CSV path, nothing else new.
 *
 * WHAT IS PRESERVED (house rule 2 — nothing dropped at a seam):
 *   - every cell of the chosen sheet, in sheet order, with gaps filled so a
 *     value in C7 lands in column 3 of line 7 even when A7/B7 do not exist;
 *   - shared strings (<si><t>) AND rich-text runs (<si><r><t>…) concatenated;
 *     phonetic <rPh> runs (Excel furigana, common in Japanese workbooks) are
 *     NOT concatenated — they are a reading aid, not the cell's text;
 *   - inline strings (t="inlineStr"), formula strings (t="str"), booleans
 *     (t="b", written as 0/1 as stored), errors (t="e", "#N/A" as stored);
 *   - numbers as the RAW stored value ("20240401", "1.5E-3"): nothing is
 *     reformatted, so a code with leading zeros stored as text stays text and
 *     a number stays the number Excel stored;
 *   - a merged region's value in its top-left cell, blanks elsewhere — that
 *     is where the file keeps it, and inventing copies would be fabrication;
 *   - rows with no <c> at all, and row numbers the file skips, come out as
 *     EMPTY lines, so line N of the CSV is row N of the sheet: a
 *     `csv_skip_lines` count somebody reads off Excel's row gutter is right.
 *
 * DATES. Excel stores a date as a serial number (45383 = 2024-04-01) and the
 * cell's number FORMAT says it is a date; that is style-sheet metadata this
 * reader does not evaluate. The serial is written verbatim, so a row whose
 * date column is a true date cell should declare it in the manifest — e.g.
 * `date_keys=登録年月日` still names the column, and the value it sees is the
 * serial: convert with `(serial - 25569) * 86400` = Unix seconds. A date typed
 * as text ("2024/4/1", "令和6年4月1日") comes through as that text.
 *
 * BOUND. A sheet whose cell count (rows × widest row, i.e. the CSV that would
 * be written) exceeds JO_XLSX_MAX_CELLS (env; default 2,000,000) is REFUSED
 * with an error — never a silently shortened CSV. A partial table that looks
 * complete is the failure this tree is built to avoid.
 *
 * NOT XLSX. A PDF renamed .xlsx, a plain ZIP, an .xls (OLE2), a ZIP without
 * xl/workbook.xml — all fail cleanly with a message naming what was seen. */
#ifndef JO_XLSX_H
#define JO_XLSX_H
#include <stddef.h>

/* Convert one worksheet of the workbook in `body` (`len` bytes, as fetched)
 * to CSV. The sheet is chosen by `sheet_name` when it is non-NULL and
 * non-empty (exact match against the workbook's sheet names), else by
 * `sheet_index` (0-based, in workbook tab order).
 *
 * On success returns 0, *csv_out = malloc'd NUL-terminated UTF-8 CSV
 * (caller frees), *csv_len = its byte length. Lines end in '\n'; a cell is
 * quoted when it holds a comma, a quote, CR or LF, with quotes doubled.
 *
 * On failure returns -1, *csv_out = NULL, and `err` (if given) holds a
 * one-line reason: "not a zip", "no xl/workbook.xml", "sheet 'x' not found
 * (have: a, b, c)", "sheet exceeds JO_XLSX_MAX_CELLS", … */
int xlsx_to_csv(const char *body, size_t len, int sheet_index,
                const char *sheet_name, char **csv_out, size_t *csv_len,
                char *err, size_t errn);

/* Sheet names, in tab order, as one '\n'-separated malloc'd string (caller
 * frees), or NULL with `err` set when `body` is not a workbook. For the
 * engine's "which sheet did you mean" messages and for the probe tool. */
char *xlsx_sheet_names(const char *body, size_t len, char *err, size_t errn);

#endif
