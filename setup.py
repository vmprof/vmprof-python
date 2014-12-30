from setuptools import setup, find_packages


setup(
    name='vmprof',
    author='vmprof team',
    author_email='sebastian.pawlus@gmail.com',
    version="0.0.3",
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
