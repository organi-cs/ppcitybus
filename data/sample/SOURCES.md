# Sample network: sources and caveats

This directory is a small, real slice of the Phnom Penh City Bus network, meant
to make `ppbus` work out of the box. It is **not** a complete or current map:
every line is marked `approx=1` until checked against the official City Bus app.

## Where the data comes from

Stops, coordinates and stop order are from **OpenStreetMap**, extracted on
2026-09-27 (Overpass data as of 2026-09-26T22:43Z).

> © OpenStreetMap contributors. Data available under the
> [Open Database License (ODbL) 1.0](https://opendatacommons.org/licenses/odbl/1-0/).
> The CSV files in this directory are a derived database and are therefore
> also under ODbL.

| Line | OSM relations | How it was built |
|---|---|---|
| 1A | 4043310, 4043311 | `tools/osm_import.py`, platform members in order |
| 2 | 4132816, 4132951 | same |
| 3 | 4133689, 4133690 | same |
| 5 | 7621535, 7621536 | by hand, see below |

Lines 1A, 2 and 3 came from:

```
python tools/osm_import.py --lines 1A,2,3 --out data/sample
```

These edits were then made by hand:
- The stop named "Central Market, ligne 3 (bleu), Stop Bus (25)" was renamed to "Central Market".
- Aliases were added to Central Market (Phsar Thmei, ផ្សារធំថ្មី, ...), Aeon Mall I and Aeon Mall II (Aeon Mall Sen Sok, AEON 2).
- A generic Khmer label ("Phnom Penh bus") was removed from node 9437232578.

### Line 5

The OSM relations for line 5 list only the stops between Aeon Mall I and
Sunsimexco, and then jump 14 km to Win-Win Boulevard. The missing section
toward Aeon Mall II (Sen Sok) was filled in from `Stop NN - ... - Bus Line 05`
nodes that exist in OSM but are not members of the relation. They were
ordered by position along the road, because OSM's stop numbers are unreliable:
a note on the stops says the numbers painted on signs and on the road disagree.

- The line ends at **Aeon Mall II**. Beyond it (toward Win-Win Boulevard or
  Prek Pnov) OSM has no stops, so the sample is marked "(partial)".
- Where a stop has one platform on each side of the road, the side used for
  each direction is a guess.
- Three stops named only "." in OSM are listed as `Unnamed stop <osm id>`.

## Known gaps

- **Line 2**: line 02 is reported to serve Aeon Mall Sen Sok, but in
  OSM it runs from Old Stadium to Prek Samrong Bridge (Takhmao). The line has
  probably been rerouted or renumbered since it was mapped, so the sample keeps
  the OSM route until the official one is available.
- No headways or travel times: `network.ini` defaults apply (15 min, 2 min).
- Only 45 of 240 stops have a Khmer name.
- The network had 21 lines as of 2024. This sample has 4.

## Other sources for the full network

- Official Phnom Penh City Bus app and City Bus Authority announcements:
  current lines and stops. Treat these as the source of truth.
- [Open Development Cambodia, "Public transportation"](https://opendevelopmentcambodia.net/profiles/access-to-public-service/public-transportation/):
  shapefiles with about 856 stops (CC-BY). Older, but good for bulk coordinates.
