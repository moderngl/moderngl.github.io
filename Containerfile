FROM docker.io/library/python:3.14 AS wheel
ARG PYODIDE_VERSION=314.0.3
RUN pip install --no-cache-dir pyodide-build && pyodide xbuildenv install ${PYODIDE_VERSION}
RUN pip download --no-deps --no-binary :all: pyglm==2.8.3 -d /sdist && tar xf /sdist/*.tar.gz -C /sdist && pyodide build /sdist/pyglm-*/ -o /wheel
RUN git clone --depth 1 https://github.com/moderngl/moderngl /src
RUN pyodide build /src -o /wheel && cd /wheel && ls *.whl > wheels.txt

FROM scratch AS site
COPY --from=wheel /wheel /
COPY index.html scene.py favicon.png logo.png /

FROM docker.io/library/nginx
RUN sed -i 's/^worker_processes.*/worker_processes 2;/' /etc/nginx/nginx.conf
COPY --from=site / /usr/share/nginx/html

# podman build . -t moderngl-site && podman run --rm -p 8000:80 moderngl-site
