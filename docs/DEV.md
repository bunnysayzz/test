# Development

```
python -m venv .venv
.venv\Scripts\activate
pip install -r tools/requirements.txt
python tools/make_icon.py
```

MSVC: `cmake -S . -B build -A x64 && cmake --build build --config Release`

MinGW: see `.github/workflows/build.yml`

Code signing: choose PFX or Azure Trusted Signing and record it here.
