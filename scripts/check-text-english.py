#!/usr/bin/env python3
"""Committed comments and user-visible strings must be English.

The incident: a commit titled "Every comment in English" left seventeen
Portuguese and French comments in the tree, across ten files. The sweep that
produced it was a hand-written grep whose word list happened not to contain
"em", "pinos", "livres" or "ecra". A manual pass was the guard, and manual
passes do not scale - which is the argument for this file in one sentence.

It also caught user-visible text that was never translated at all: a printer
whose brand is unknown rendered as "marca#1234" on the panel, and the printer
type list on the legacy page opened with "Nenhuma".

HOW IT WORKS, and its limits. This is a marker-word heuristic, not a language
classifier: it looks for function words that are common in Portuguese, French,
Spanish, Italian and German and that are not English words. It will not catch a
single non-English noun in an otherwise English sentence. It is meant to catch
prose, which is what actually gets committed in the wrong language.

Words that collide with English are deliberately absent from the list - "a",
"no", "do", "as", "e", "sale", "pain" and their kind - because a guard with
false positives is a guard people learn to bypass.

SCOPE:

  - Comments in firmware/src, firmware/include, firmware/tools and scripts.
  - String literals in the same, minus the three regions that are legitimately
    multilingual: the i18n table and language names, the legacy page's
    translation table, and the captive portal, which carries eight languages of
    HTML on purpose.
  - EXCLUDED: firmware/lib - vendored third-party code we do not rewrite.
"""

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cxx_scan  # noqa: E402

REPO = pathlib.Path(__file__).resolve().parent.parent

# Function words common in the languages this project drifts into, chosen so
# that none is also an English word.
MARKERS = {
    # Portuguese / Spanish
    "nao", "não", "com", "sem", "para", "pelo", "pela", "uma", "isso", "aqui",
    "ficar", "pinos", "livres", "ecra", "ecrã", "tabelas", "referencia",
    "ordenadas", "materiais", "marcas", "marca", "arranque", "resposta",
    "objecto", "seus", "suportado", "catalogo", "codigo", "descoberta",
    "detetada", "errados", "espelhado", "espera", "externo", "fluxo", "manda",
    "melhor", "mostra", "nunca", "paralelos", "pareamento", "pode", "preenche",
    "recarrega", "unidades", "gera", "ajusta", "ausente", "branco", "chega",
    "como", "esta", "falhou", "falha", "nenhuma", "ligar", "aguardar",
    "aprovacao", "enviado", "erro", "cama", "guardar", "tela", "arquivo",
    "sao", "sua", "seu", "seja", "entao", "apos", "ultima", "primeiro",
    # Nouns and participles, not function words. Every one of these was found on
    # a screen or in a log AFTER a sweep declared the tree clean: the list is a
    # heuristic and grows by being wrong in public.
    "ignoradas", "ignorada", "relatorio", "suporte", "unidade", "palpite",
    "estacao", "maquinas", "telemovel", "ecrã", "aguardar", "alteracoes",
    "pendente", "transitoria", "credenciais", "ligacoes", "ligacao",
    "impressora", "impressoras", "tabuleiros", "marca",
    # French
    "les", "des", "une", "dans", "pour", "avec", "qui", "cette", "tout",
    "toute", "meme", "même", "cote", "côté", "ecriture", "boucle", "dessin",
    "jetes", "reseau", "echouerait", "designeraient", "liens", "symboles",
    "sont", "leur", "notre", "chaque", "aussi", "encore", "depuis", "apres",
    "ainsi", "donc", "vers", "sous", "entre", "toujours", "jamais",
    "pas", "doit", "doivent", "peut", "peuvent", "ceci", "cela", "elle",
    "nous", "vous", "faut", "alors", "quand", "fait", "etre", "être",
    "avoir", "afin", "plutot", "plutôt", "ici", "deja", "déjà", "trop",
    "demarrage", "démarrage", "ecran", "écran", "fichier", "ligne",
    # Italian / German prose markers
    "nicht", "oder", "eine", "werden", "sich", "auch", "wird",
    "questo", "della", "delle", "sono", "anche",
}

WORD = re.compile(r"[A-Za-zÀ-ÿ]+")

# URLs and e-mail addresses are identifiers, not prose, and every ".com" in one
# reads as the Portuguese word. Removed before the text is tokenised: a guard
# that cries wolf on a Google API endpoint is a guard people learn to bypass.
#
# Bare hostnames and dotted filenames count too. "us.mqtt.bambulab.com" has no
# scheme to spot it by, and its last label reads as the Portuguese "com" - so a
# broker address in a string looked like Portuguese prose. Two or more
# dot-joined labels with no spaces is never a sentence.
NOT_PROSE = re.compile(
    r"https?://\S+"
    r"|\b[\w.+-]+@[\w.-]+\.\w+"
    r"|\.?(?:[A-Za-z0-9_-]+\.){2,}[A-Za-z]{2,6}\b")

# Regions that are supposed to hold other languages. Excised before scanning
# rather than excluding the whole file, so the rest of the file is still covered.
MULTILINGUAL = {
    "firmware/src/i18n.cpp": [
        r"static const Row STR\[S_COUNT\] = \{.*?\n\};",
        r"static const char\* const NAMES\[LANG_N\] = \{.*?\};",
    ],
    "firmware/src/webcfg.cpp": [
        r"const char\* const WT\[W_N\]\[\d+\] = \{.*?\n    \};",
    ],
}

SKIP_FILES = {
    # Eight languages of HTML, on purpose.
    "firmware/src/net/portal_page.h",
    # This file is a list of non-English words by construction, and quotes the
    # words it was written to catch. Scanning it finds itself, every time.
    "scripts/check-text-english.py",
}


def comments_and_literals(text: str, is_python: bool):
    """Yield (line_number, kind, snippet) for every comment and string."""
    if is_python:
        for i, line in enumerate(text.splitlines(), 1):
            m = re.search(r"#(.*)$", line)
            if m:
                yield i, "comment", m.group(1)
            for lit in re.findall(r'"([^"\n]*)"|\'([^\'\n]*)\'', line):
                s = lit[0] or lit[1]
                if s:
                    yield i, "string", s
        # Module and function docstrings.
        for m in re.finditer(r'"""(.*?)"""', text, re.S):
            yield text[:m.start()].count("\n") + 1, "docstring", m.group(1)
        return

    for kind, lineno, content in cxx_scan.scan(text):
        if content.strip():
            yield lineno, kind, content


def files():
    for root, patterns in (("firmware/src", ("*.cpp", "*.h")),
                           ("firmware/include", ("*.h",)),
                           ("firmware/tools", ("*.py",)),
                           ("scripts", ("*.py", "*.sh"))):
        for pat in patterns:
            yield from sorted((REPO / root).rglob(pat))


def main() -> int:
    problems = []
    scanned = 0

    for path in files():
        rel = path.relative_to(REPO).as_posix()
        if rel in SKIP_FILES or "/lib/" in f"/{rel}":
            continue
        scanned += 1
        text = path.read_text(encoding="utf-8", errors="replace")
        for pattern in MULTILINGUAL.get(rel, []):
            # Blank the region but keep its newlines, so line numbers hold.
            text = re.sub(pattern,
                          lambda m: "\n" * m.group(0).count("\n"),
                          text, flags=re.S)

        for lineno, kind, snippet in comments_and_literals(text, path.suffix == ".py"):
            prose = NOT_PROSE.sub(" ", snippet)
            hits = sorted({w.lower() for w in WORD.findall(prose)
                           if w.lower() in MARKERS})
            if hits:
                problems.append(
                    f"{rel}:{lineno}: non-English {kind} "
                    f"({', '.join(hits)}): {snippet.strip()[:70]}")

    if scanned == 0:
        print("error: scanned no files - this check is checking nothing",
              file=sys.stderr)
        return 2

    for p in problems:
        print(f"error: {p}", file=sys.stderr)
    print(f"scanned {scanned} files, {len(problems)} violations")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
