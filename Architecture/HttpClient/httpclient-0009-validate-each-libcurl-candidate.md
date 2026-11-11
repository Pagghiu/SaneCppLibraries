# HTTPCLIENT-0009 - Validate Each Libcurl Candidate

Status: Accepted

## Context

The dynamic loader can open a library whose exported symbols are insufficient for HttpClient.
A failed symbol lookup leaves any already-resolved addresses tied to a library that is about to be closed.

## Decision

Try each supported library name until a candidate opens and exports every required symbol. Close
incomplete candidates and clear their function pointers before trying the next name. Clear the
pointers on explicit close as well.

## Confirmation

Run the existing HttpClientTest in separate processes with missing, incomplete, and complete
fixture libraries. Cover independent TLS and HTTP/2 feature flags and rejection of a missing
runtime through the public initialization API.
