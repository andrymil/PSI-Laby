FROM python:3.9-slim

# --- POPRAWKA ---
# Instalujemy pakiet 'iproute2', który zawiera program 'tc'
# (jest to niezbędne do symulacji gubienia pakietów).
# czyścimy cache apt, aby nie powiększać niepotrzebnie obrazu.
RUN apt-get update && \
    apt-get install -y iproute2 && \
    rm -rf /var/lib/apt/lists/*
# ---------------

COPY . /Client
WORKDIR /Client/