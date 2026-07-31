window.BENCHMARK_DATA = {
  "lastUpdate": 1785531834060,
  "repoUrl": "https://github.com/mangrisano/jpick",
  "entries": {
    "jpick benchmarks": [
      {
        "commit": {
          "author": {
            "email": "michele.angrisano@gmail.com",
            "name": "Michele Angrisano",
            "username": "mangrisano"
          },
          "committer": {
            "email": "michele.angrisano@gmail.com",
            "name": "Michele Angrisano",
            "username": "mangrisano"
          },
          "distinct": true,
          "id": "4939ed709573d63a6730bb4eea32df0fd7baab52",
          "message": "ci: add performance benchmark workflow and badge",
          "timestamp": "2026-07-31T22:58:04+02:00",
          "tree_id": "0f4ea44663c8ab8c85ec20c402ae0a0b1d4f8923",
          "url": "https://github.com/mangrisano/jpick/commit/4939ed709573d63a6730bb4eea32df0fd7baab52"
        },
        "date": 1785531833230,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "iterate + pipe (100000 objects)",
            "value": 511.501,
            "unit": "ms"
          },
          {
            "name": "pretty-print (100000 objects)",
            "value": 537.398,
            "unit": "ms"
          },
          {
            "name": "index + field (100000 objects)",
            "value": 413.604,
            "unit": "ms"
          }
        ]
      }
    ]
  }
}