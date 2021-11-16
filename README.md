# Dynamically Detecting and Reducing False Sharing
*EECS 583 F21 &mdash; Group 21*

*Tony Bai, Daniel Hoekwater, Brandon Kayes, Thomas Smith*

## Organization
- `bench` - Benchmark programs that exhibit false sharing
- `src`   - Source code for the compiler passes
  - `profile` - First (analysis) pass
  - `fix`     - Second pass to fix false sharing
