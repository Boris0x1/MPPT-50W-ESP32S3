function fijar_configuracion(mw,cfg)
assert(numel(cfg)==15 && all(isfinite(cfg)));
assignin(mw,'ConfigControl',cfg);
assignin(mw,'cfg_profile',timeseries([cfg;cfg],[0;6.5]));
end
