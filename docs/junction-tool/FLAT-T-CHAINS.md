# The flat T of the test spec as chains (N0b, ExtractJunctionChains, layout coordinates after the crop)
Format: C<unit><L|R>@x,y curve; S<unit>@x,y split; M<unit>[p]@x,y merge (p = prio). Units: axis piece 2, half piece 1.
```
EIn->SOut units 71: C46R@23,1 S46@23,1 C51R@25,4 C57L@25,7 C63R@28,10 M65@28,11
EIn->EOut units 82: S46@23,1 M76p@38,1
WIn->WOut units 82: S14@33,3 S18@31,3 C36L@22,3 M37@22,4 C38R@21,4 M68p@6,4 C76R@2,4 C78L@1,3
WIn->WOut units 82: S14@33,3 C18L@31,3 S18@31,3 C20R@30,4 M36@22,4 M68p@6,4 C76R@2,4 C78L@1,3
WIn->SOut units 35: C14L@33,3 S14@33,3 C19L@31,6 C25R@31,9 M30@28,11 C31L@28,12
NIn->EOut units 47: S4@26,11 C18R@26,4 C23R@29,2 C41L@38,2 M42@38,1 C43R@39,1
NIn->WOut units 57: C4L@26,11 S4@26,11 C17L@19,5 C43R@6,5 M44@6,4 C45L@5,4 C51R@2,4 C53L@1,3
```
Roles here: EIn->EOut is the main line the RECON calls A; WIn->WOut is B (two lane paths).
