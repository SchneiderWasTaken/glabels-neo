// OpenWhisk (DigitalOcean Functions) entry point.  The relay itself is ES modules; load it lazily.
exports.main = async (args) => (await import('./adapter.mjs')).main(args);
