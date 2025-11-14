# Używamy oficjalnego, lekkiego obrazu Python
FROM python:3.9-slim

COPY . /Server
WORKDIR /Server/

# Nie ma potrzeby niczego instalować ani kompilować
# Uruchamiamy bezpośrednio skrypt serwera
CMD ["python3", "./server.py"]