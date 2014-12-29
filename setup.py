from setuptools import setup, find_packages


setup(
    name='vmprof',
    author='vmprof team',
    author_email='info@divio.ch',
    version="0.0.1",
    packages=find_packages(),
    description="Python's vmprof client",
    install_requires=[
        "requests==2.5.1"
    ],
    classifiers=[
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python',
    ],
    zip_safe=False
)
