

check if this PR is correct:
- https://github.com/z-libs/zstr.h/pull/5

- test everything to 100%, as this library is used in many C libraries of mine
- especially SSO overflow and the corner cases with 22, 23 and 24 length, is capacity and length correct? is tstr_cstr() always \0 delimited


- implement the no short strings mode better
