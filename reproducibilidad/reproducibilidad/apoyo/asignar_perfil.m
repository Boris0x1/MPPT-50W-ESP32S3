function asignar_perfil(mw,p)
% Preserve the exact matrix format and 1 ms time grid of the original bank.
assignin(mw,'G_profile',[p.t double(p.G)]);
assignin(mw,'R_profile',[p.t double(p.R)]);
assignin(mw,'Pref_profile',[p.t double(p.reference)]);
assignin(mw,'T_profile',[0 25;p.duration 25]);
end
